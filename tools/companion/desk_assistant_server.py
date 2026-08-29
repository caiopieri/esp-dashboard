#!/usr/bin/env python3
"""Local control-plane bridge for Desk Assistant.

The ESP32 receives bounded, declarative card manifests. This companion keeps
provider credentials off the device and turns an optional AI response into a
validated draft for Card Studio. It binds to localhost by default.
"""

from __future__ import annotations

import json
import os
import re
import secrets
import subprocess
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib import error, request
from urllib.parse import urlparse

ALLOWED_TYPES = {"text", "metric", "progress", "status", "clock", "list", "chart"}
ID_RE = re.compile(r"^[A-Za-z0-9_-]{1,32}$")
HEX_RE = re.compile(r"^#[0-9a-fA-F]{6}$")
ACTION_ID_RE = re.compile(r"^[A-Za-z0-9_-]{1,32}$")
MAX_PROMPT_BYTES = 4096
MAX_ACTIONS = 32
ACTION_CONFIRMATION_SECONDS = 30
ALLOWED_DISPLAYS = {"320x240", "480x320", "e-ink"}

_ACTION_CHALLENGES: dict[str, tuple[str, float]] = {}
_ACTION_LOCK = threading.Lock()


class CardValidationError(ValueError):
    pass


class ActionError(ValueError):
    pass


class ActionExecutionError(RuntimeError):
    pass


def _string(value: Any, field: str, limit: int, default: str = "") -> str:
    if value is None:
        value = default
    if not isinstance(value, str) or len(value) > limit:
        raise CardValidationError(f"{field} must be a string of at most {limit} characters")
    return value


def validate_card(raw: Any) -> dict[str, Any]:
    """Normalize one AI/user draft using the same bounded contract as firmware."""

    if not isinstance(raw, dict):
        raise CardValidationError("card must be an object")
    card_id = _string(raw.get("id"), "id", 32)
    if not ID_RE.fullmatch(card_id):
        raise CardValidationError("id must contain only A-Z, a-z, 0-9, _ or -")
    title = _string(raw.get("title"), "title", 64, card_id)
    card_type = _string(raw.get("type"), "type", 16, "metric")
    if card_type not in ALLOWED_TYPES:
        raise CardValidationError(f"unsupported type: {card_type}")

    data = raw.get("data") if isinstance(raw.get("data"), dict) else {}
    source = _string(data.get("source"), "data.source", 16, "static")
    if source not in {"static", "runtime", "variable"}:
        raise CardValidationError("data.source must be static, runtime or variable")
    namespace = _string(data.get("namespace"), "data.namespace", 32, card_id)
    key = _string(data.get("key"), "data.key", 32, "value")
    value = _string(data.get("value"), "data.value", 256, "--")

    body = raw.get("body") if isinstance(raw.get("body"), dict) else {}
    label = _string(body.get("label"), "body.label", 64)
    unit = _string(body.get("unit"), "body.unit", 16)
    maximum = body.get("max", 100)
    if isinstance(maximum, bool) or not isinstance(maximum, (int, float)):
        raise CardValidationError("body.max must be numeric")
    maximum = max(1, min(100000, int(maximum)))

    items = body.get("items", [])
    if items is None:
        items = []
    if not isinstance(items, list) or len(items) > 8:
        raise CardValidationError("body.items must contain at most 8 strings")
    clean_items = [_string(item, "body.items[]", 64) for item in items]

    theme = raw.get("theme") if isinstance(raw.get("theme"), dict) else {}
    accent = _string(theme.get("accent"), "theme.accent", 7, "#74f0c1")
    if not HEX_RE.fullmatch(accent):
        raise CardValidationError("theme.accent must be a #RRGGBB color")

    normalized = {
        "schemaVersion": 1,
        "id": card_id,
        "title": title,
        "kind": "declarative",
        "type": card_type,
        "enabled": True,
        "deleted": False,
        "order": 0,
        "data": {
            "source": source,
            "namespace": namespace,
            "key": key,
            "value": value,
        },
        "body": {"label": label, "unit": unit, "max": maximum, "items": clean_items},
        "theme": {"accent": accent},
    }
    action = raw.get("action")
    if action is not None:
        if not isinstance(action, dict):
            raise CardValidationError("action must be an object")
        action_id = _string(action.get("id"), "action.id", 32)
        if not ACTION_ID_RE.fullmatch(action_id):
            raise CardValidationError("action.id contains invalid characters")
        action_label = _string(action.get("label"), "action.label", 32, "Executar")
        confirmation = action.get("confirmationRequired", True)
        if not isinstance(confirmation, bool):
            raise CardValidationError("action.confirmationRequired must be boolean")
        normalized["action"] = {
            "id": action_id,
            "label": action_label,
            "confirmationRequired": confirmation,
        }
    return normalized


def load_actions() -> dict[str, dict[str, Any]]:
    """Load trusted local action definitions; cards can reference IDs only."""

    filename = os.environ.get("DESK_ACTIONS_FILE", "").strip()
    if not filename:
        return {}
    try:
        raw = json.loads(Path(filename).read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ActionError(f"invalid actions file: {exc}") from exc
    if not isinstance(raw, dict) or not isinstance(raw.get("actions"), list):
        raise ActionError("actions file must contain an actions array")
    if len(raw["actions"]) > MAX_ACTIONS:
        raise ActionError(f"at most {MAX_ACTIONS} actions are allowed")

    actions: dict[str, dict[str, Any]] = {}
    for item in raw["actions"]:
        if not isinstance(item, dict):
            raise ActionError("each action must be an object")
        action_id = _string(item.get("id"), "action.id", 32)
        if not ACTION_ID_RE.fullmatch(action_id) or action_id in actions:
            raise ActionError("action IDs must be unique and use A-Z, 0-9, _ or -")
        title = _string(item.get("title"), "action.title", 64, action_id)
        kind = _string(item.get("kind"), "action.kind", 16)
        if kind not in {"open_url", "launch"}:
            raise ActionError("action.kind must be open_url or launch")
        confirmation = item.get("confirmationRequired", True)
        if not isinstance(confirmation, bool):
            raise ActionError("action.confirmationRequired must be boolean")

        clean: dict[str, Any] = {
            "id": action_id,
            "title": title,
            "kind": kind,
            "confirmationRequired": confirmation,
        }
        if kind == "open_url":
            url = _string(item.get("url"), "action.url", 512)
            parsed = urlparse(url)
            if parsed.scheme not in {"http", "https"} or not parsed.hostname or parsed.username or parsed.password:
                raise ActionError("open_url requires an http(s) URL without embedded credentials")
            allowed_hosts = item.get("allowedHosts", [])
            if not isinstance(allowed_hosts, list) or any(not isinstance(host, str) for host in allowed_hosts):
                raise ActionError("allowedHosts must be a list of host names")
            if allowed_hosts and parsed.hostname.lower() not in {host.lower() for host in allowed_hosts}:
                raise ActionError("URL host is not in allowedHosts")
            clean["url"] = url
        else:
            command = item.get("command")
            if not isinstance(command, list) or not command or len(command) > 8:
                raise ActionError("launch command must contain 1 to 8 arguments")
            if any(not isinstance(part, str) or not part or len(part) > 256 for part in command):
                raise ActionError("launch command arguments are invalid")
            clean["command"] = command
        actions[action_id] = clean
    return actions


def validate_card_package(raw: Any) -> dict[str, Any]:
    """Validate the signed/catalogue-ready envelope around one card."""

    if not isinstance(raw, dict) or raw.get("kind") != "desk-assistant-card":
        raise CardValidationError("package.kind must be desk-assistant-card")
    minimum_schema = raw.get("minimumSchemaVersion", 1)
    if (
        raw.get("packageVersion") != 1
        or isinstance(minimum_schema, bool)
        or not isinstance(minimum_schema, int)
        or minimum_schema > 1
    ):
        raise CardValidationError("unsupported card package version")
    compatibility = raw.get("compatibility")
    if not isinstance(compatibility, dict) or not isinstance(compatibility.get("displays"), list):
        raise CardValidationError("package.compatibility.displays is required")
    displays = compatibility["displays"]
    if not displays or len(displays) > len(ALLOWED_DISPLAYS) or any(display not in ALLOWED_DISPLAYS for display in displays):
        raise CardValidationError("package contains an unsupported display profile")
    return {
        "packageVersion": 1,
        "kind": "desk-assistant-card",
        "minimumSchemaVersion": 1,
        "compatibility": {"displays": list(dict.fromkeys(displays))},
        "card": validate_card(raw.get("card")),
    }


def public_actions() -> list[dict[str, Any]]:
    return [
        {
            "id": action["id"],
            "title": action["title"],
            "kind": action["kind"],
            "confirmationRequired": action["confirmationRequired"],
        }
        for action in load_actions().values()
    ]


def _execute_configured_action(action: dict[str, Any]) -> None:
    if action["kind"] == "open_url":
        if not webbrowser.open(action["url"], new=2):
            raise ActionExecutionError("browser rejected the URL")
        return
    # The command comes exclusively from DESK_ACTIONS_FILE and is never
    # concatenated or passed through a shell. Card input cannot add arguments.
    try:
        subprocess.Popen(
            action["command"],
            shell=False,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
    except OSError as exc:
        raise ActionExecutionError("configured launch command could not start") from exc


def execute_action_request(action_id: Any, confirmation_token: Any = None) -> dict[str, Any]:
    """Execute one configured action after a short-lived explicit confirmation."""

    if not isinstance(action_id, str) or not ACTION_ID_RE.fullmatch(action_id):
        raise ActionError("invalid action id")
    actions = load_actions()
    action = actions.get(action_id)
    if action is None:
        raise ActionError("action is not configured")

    if action["confirmationRequired"]:
        if not confirmation_token:
            token = secrets.token_urlsafe(24)
            with _ACTION_LOCK:
                now = time.monotonic()
                for old_token, (_, expiry) in list(_ACTION_CHALLENGES.items()):
                    if expiry <= now:
                        del _ACTION_CHALLENGES[old_token]
                _ACTION_CHALLENGES[token] = (action_id, now + ACTION_CONFIRMATION_SECONDS)
            return {
                "status": "confirmation_required",
                "id": action_id,
                "title": action["title"],
                "confirmationToken": token,
                "expiresInSeconds": ACTION_CONFIRMATION_SECONDS,
            }
        with _ACTION_LOCK:
            challenge = _ACTION_CHALLENGES.pop(str(confirmation_token), None)
        if challenge is None or challenge[0] != action_id or challenge[1] <= time.monotonic():
            raise ActionError("confirmation token is invalid or expired")

    _execute_configured_action(action)
    return {"status": "executed", "id": action_id}


def _extract_json(text: str) -> Any:
    """Accept strict JSON and the fenced JSON frequently returned by models."""

    candidate = text.strip()
    if candidate.startswith("```"):
        candidate = re.sub(r"^```(?:json)?\s*", "", candidate, flags=re.IGNORECASE)
        candidate = re.sub(r"\s*```$", "", candidate)
    try:
        return json.loads(candidate)
    except json.JSONDecodeError as exc:
        raise CardValidationError("AI response was not valid JSON") from exc


def _ai_payload(prompt: str) -> dict[str, Any]:
    system = (
        "You design safe embedded dashboard cards. Return only a JSON object with "
        "a card field. The card must use exactly one of these types: text, metric, "
        "progress, status, clock, list, chart. Use at most 32 chars for id, 64 "
        "for title/label, 16 for unit, 8 list items, and no URLs, secrets, code, "
        "network requests, shell commands, or arbitrary actions. Use data.source "
        "static, runtime, or variable. The result must fit a 320x240 display."
    )
    return {
        "model": os.environ.get("DESK_AI_MODEL", "gpt-4o-mini"),
        "temperature": 0.2,
        "messages": [
            {"role": "system", "content": system},
            {"role": "user", "content": prompt},
        ],
    }


def generate_draft(prompt: str) -> dict[str, Any]:
    if not isinstance(prompt, str) or not prompt.strip():
        raise CardValidationError("prompt is required")
    if len(prompt.encode("utf-8")) > MAX_PROMPT_BYTES:
        raise CardValidationError("prompt is too large")

    endpoint = os.environ.get("DESK_AI_ENDPOINT", "").strip()
    api_key = os.environ.get("DESK_AI_API_KEY", "").strip()
    if not endpoint or not api_key:
        raise RuntimeError(
            "AI connector is not configured; set DESK_AI_ENDPOINT and DESK_AI_API_KEY"
        )

    payload = json.dumps(_ai_payload(prompt)).encode("utf-8")
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {api_key}",
    }
    try:
        with request.urlopen(request.Request(endpoint, data=payload, headers=headers, method="POST"), timeout=30) as response:
            result = json.loads(response.read(64 * 1024).decode("utf-8"))
    except (error.URLError, TimeoutError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"AI provider request failed: {exc}") from exc

    try:
        content = result["choices"][0]["message"]["content"]
    except (KeyError, IndexError, TypeError) as exc:
        raise RuntimeError("AI provider response has no chat message") from exc
    decoded = _extract_json(content)
    raw_card = decoded.get("card") if isinstance(decoded, dict) else None
    return {"card": validate_card(raw_card), "source": "ai", "schemaVersion": 1}


class CompanionHandler(BaseHTTPRequestHandler):
    server_version = "DeskAssistantCompanion/0.1"

    def _send(self, status: int, payload: dict[str, Any]) -> None:
        encoded = b"" if status == 204 else json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.end_headers()
        self.wfile.write(encoded)

    def do_OPTIONS(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        self._send(204, {})

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path == "/healthz":
            self._send(200, {"ok": True, "service": "desk-assistant-companion", "version": 1})
            return
        if self.path == "/api/actions":
            try:
                self._send(200, {"schemaVersion": 1, "actions": public_actions()})
            except ActionError as exc:
                self._send(503, {"error": "actions_unavailable", "detail": str(exc)})
            return
        self._send(404, {"error": "not_found"})

    def do_POST(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self._send(400, {"error": "invalid_content_length"})
            return
        if length <= 0 or length > 8192:
            self._send(413, {"error": "request_too_large"})
            return
        try:
            body = json.loads(self.rfile.read(length).decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            self._send(400, {"error": "invalid_json"})
            return
        if not isinstance(body, dict):
            self._send(400, {"error": "json_object_required"})
            return

        try:
            if self.path == "/api/cards/validate":
                self._send(200, {"valid": True, "card": validate_card(body.get("card", body))})
                return
            if self.path == "/api/cards/package/validate":
                self._send(200, {"valid": True, "package": validate_card_package(body)})
                return
            if self.path == "/api/ai/draft":
                self._send(200, generate_draft(body.get("prompt", "")))
                return
            if self.path == "/api/actions/execute":
                result = execute_action_request(body.get("id"), body.get("confirmationToken"))
                self._send(409 if result["status"] == "confirmation_required" else 200, result)
                return
            self._send(404, {"error": "not_found"})
        except CardValidationError as exc:
            self._send(400, {"error": "invalid_card", "detail": str(exc)})
        except ActionError as exc:
            self._send(400, {"error": "invalid_action", "detail": str(exc)})
        except ActionExecutionError as exc:
            self._send(502, {"error": "action_failed", "detail": str(exc)})
        except RuntimeError as exc:
            self._send(503, {"error": "ai_unavailable", "detail": str(exc)})

    def log_message(self, fmt: str, *args: Any) -> None:
        print(f"[companion] {fmt % args}", file=sys.stderr)


def main() -> None:
    host = os.environ.get("DESK_COMPANION_HOST", "127.0.0.1")
    port = int(os.environ.get("DESK_COMPANION_PORT", "8787"))
    server = ThreadingHTTPServer((host, port), CompanionHandler)
    print(f"Desk Assistant companion listening on http://{host}:{port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping companion")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
