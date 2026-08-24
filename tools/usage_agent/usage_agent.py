#!/usr/bin/env python3
"""Lightweight host agent for ESP Dashboard provider usage.

The agent is intentionally outbound-only: it polls enabled providers and sends
normalized snapshots to the ESP over HTTP. Provider secrets stay in the host's
Keychain/environment and never enter the ESP configuration.
"""

import argparse
import getpass
import json
import os
import plistlib
import re
import sqlite3
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Dict, Mapping, Optional
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen


APP_LABEL = "com.caiopieri.esp-dashboard-usage-agent"
DEFAULT_INTERVAL = 60
DEFAULT_CONFIG = {
    "device_url": "http://192.168.15.64",
    "interval_seconds": DEFAULT_INTERVAL,
    "providers": {
        "claude": {
            "enabled": False,
            "keychain_service": "Claude Code-credentials",
            "model": "claude-haiku-4-5-20251001",
        },
        "openai": {
            "enabled": False,
            "admin_key_env": "OPENAI_ADMIN_KEY",
            "keychain_service": "ESP Dashboard OpenAI Admin Key",
        },
        "gemini": {
            "enabled": False,
            "snapshot_file": "",
        },
        "omniroute": {
            "enabled": True,
            "database": "~/.omniroute/storage.sqlite",
            "max_age_seconds": 600,
            "claude_providers": ["claude"],
            "gpt_providers": ["codex"],
            "gemini_providers": ["agy", "antigravity"],
            "gemini_model": "",
        },
    },
}


class AgentError(Exception):
    pass


def log(message: str) -> None:
    print("[%s] %s" % (time.strftime("%H:%M:%S"), message), flush=True)


def config_dir() -> Path:
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Application Support" / "esp-dashboard-usage-agent"
    return Path.home() / ".config" / "esp-dashboard-usage-agent"


def config_path() -> Path:
    return config_dir() / "config.json"


def log_path() -> Path:
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Logs" / "esp-dashboard-usage-agent.log"
    return config_dir() / "agent.log"


def merge_defaults(value: Mapping[str, Any], defaults: Mapping[str, Any]) -> Dict[str, Any]:
    result: Dict[str, Any] = dict(defaults)
    for key, item in value.items():
        if isinstance(item, Mapping) and isinstance(result.get(key), Mapping):
            result[key] = merge_defaults(item, result[key])  # type: ignore[arg-type]
        else:
            result[key] = item
    return result


def load_config(path: Optional[Path] = None) -> Dict[str, Any]:
    target = path or config_path()
    if not target.exists():
        return dict(DEFAULT_CONFIG)
    try:
        raw = json.loads(target.read_text())
    except (OSError, ValueError) as exc:
        raise AgentError("configuração inválida: %s" % exc)
    if not isinstance(raw, Mapping):
        raise AgentError("configuração precisa ser um objeto JSON")
    return merge_defaults(raw, DEFAULT_CONFIG)


def save_config(config: Mapping[str, Any], path: Optional[Path] = None) -> Path:
    target = path or config_path()
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(config, indent=2, ensure_ascii=False) + "\n")
    os.chmod(target, 0o600)
    return target


def _decode_keychain_blob(raw: str) -> str:
    value = raw.strip()
    if value and len(value) % 2 == 0 and re.fullmatch(r"[0-9a-fA-F]+", value):
        try:
            return bytes.fromhex(value).decode("utf-8")
        except (ValueError, UnicodeDecodeError):
            pass
    return raw


def read_keychain(service: str, account: Optional[str] = None) -> Optional[str]:
    if sys.platform != "darwin" or not service:
        return None
    command = ["security", "find-generic-password", "-s", service]
    if account:
        command.extend(["-a", account])
    command.append("-w")
    try:
        result = subprocess.run(command, check=True, capture_output=True, text=True, timeout=10)
    except (OSError, subprocess.SubprocessError):
        return None
    return _decode_keychain_blob(result.stdout).strip() or None


def secret_for(provider: Mapping[str, Any], env_key: str = "") -> Optional[str]:
    if env_key:
        value = os.environ.get(env_key)
        if value:
            return value
    service = str(provider.get("keychain_service", ""))
    account = str(provider.get("keychain_account", getpass.getuser()))
    return read_keychain(service, account)


def extract_access_token(blob: str) -> Optional[str]:
    value = blob.strip()
    if not value:
        return None
    try:
        data = json.loads(value)
    except ValueError:
        data = None
    if isinstance(data, Mapping):
        direct = data.get("accessToken")
        if isinstance(direct, str):
            return direct
        for item in data.values():
            if isinstance(item, Mapping) and isinstance(item.get("accessToken"), str):
                return item["accessToken"]
    match = re.search(r'"accessToken"\s*:\s*"([^"]+)"', value)
    if match:
        return match.group(1)
    if re.fullmatch(r"[A-Za-z0-9_\-.~+/=]{20,}", value):
        return value
    return None


def http_json(url: str, method: str = "GET", headers: Optional[Mapping[str, str]] = None,
              body: Optional[Mapping[str, Any]] = None, timeout: int = 20) -> Mapping[str, Any]:
    data = None
    request_headers = dict(headers or {})
    if body is not None:
        data = json.dumps(body, separators=(",", ":")).encode("utf-8")
        request_headers.setdefault("Content-Type", "application/json")
    request = Request(url, data=data, headers=request_headers, method=method)
    try:
        with urlopen(request, timeout=timeout) as response:
            raw = response.read().decode("utf-8")
            return {"_headers": dict(response.headers.items()), **json.loads(raw or "{}")}
    except HTTPError as exc:
        detail = exc.read().decode("utf-8", "replace")[:240]
        raise AgentError("%s retornou HTTP %d: %s" % (url, exc.code, detail))
    except (URLError, TimeoutError, OSError, ValueError) as exc:
        raise AgentError("falha HTTP em %s: %s" % (url, exc))


def http_form(url: str, fields: Mapping[str, Any], timeout: int = 20) -> Mapping[str, Any]:
    request = Request(
        url,
        data=urlencode(fields).encode("utf-8"),
        headers={"Content-Type": "application/x-www-form-urlencoded"},
        method="POST",
    )
    try:
        with urlopen(request, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8") or "{}")
    except HTTPError as exc:
        detail = exc.read().decode("utf-8", "replace")[:240]
        raise AgentError("%s retornou HTTP %d: %s" % (url, exc.code, detail))
    except (URLError, TimeoutError, OSError, ValueError) as exc:
        raise AgentError("falha HTTP em %s: %s" % (url, exc))


def header(headers: Mapping[str, str], name: str, default: str = "") -> str:
    wanted = name.lower()
    for key, value in headers.items():
        if key.lower() == wanted:
            return str(value)
    return default


def percent(value: str) -> int:
    try:
        number = float(value)
    except (TypeError, ValueError):
        return 0
    if number <= 1:
        number *= 100
    return max(0, min(100, int(round(number))))


def reset_minutes(value: str, now: Optional[float] = None) -> int:
    try:
        remaining = float(value) - (now or time.time())
    except (TypeError, ValueError):
        return 0
    return max(0, int(round(remaining / 60)))


def reset_minutes_from_iso(value: Any, now: Optional[float] = None) -> int:
    if not isinstance(value, str) or not value:
        return -1
    try:
        normalized = value.replace("Z", "+00:00")
        from datetime import datetime, timezone
        target = datetime.fromisoformat(normalized)
        if target.tzinfo is None:
            target = target.replace(tzinfo=timezone.utc)
        current = datetime.fromtimestamp(now or time.time(), tz=timezone.utc)
        return max(0, int(round((target - current).total_seconds() / 60)))
    except (TypeError, ValueError, OverflowError):
        return -1


def quota_used_percent(value: Any) -> int:
    try:
        remaining = float(value)
    except (TypeError, ValueError):
        return -1
    return max(0, min(100, int(round(100 - remaining))))


def _latest_rows(rows: list[Mapping[str, Any]]) -> Dict[tuple[str, str], Mapping[str, Any]]:
    latest: Dict[tuple[str, str], Mapping[str, Any]] = {}
    for row in rows:
        key = (str(row.get("provider", "")), str(row.get("window_key", "")))
        if key[0] and key[1] and key not in latest:
            latest[key] = row
    return latest


def _providers_from_config(value: Any, fallback: list[str]) -> list[str]:
    if not isinstance(value, list):
        return fallback
    return [str(item) for item in value if str(item)] or fallback


def _attach_today_metrics(snapshot: Dict[str, Any], provider: str,
                          metrics: Mapping[str, Mapping[str, int]], sources: list[str]) -> None:
    matching = [metrics[name] for name in sources if name in metrics]
    if matching:
        snapshot["tokens"] = str(sum(item["tokens"] for item in matching))
        snapshot["requests"] = str(sum(item["requests"] for item in matching))


def build_omniroute_snapshots(rows: list[Mapping[str, Any]], settings: Mapping[str, Any],
                              metrics: Optional[Mapping[str, Mapping[str, int]]] = None) -> Dict[str, Dict[str, Any]]:
    """Convert OmniRoute's remaining-quota rows into ESP usage snapshots."""
    latest = _latest_rows(rows)
    result: Dict[str, Dict[str, Any]] = {}

    claude_providers = _providers_from_config(settings.get("claude_providers"), ["claude"])
    claude_rows = []
    for provider in claude_providers:
        for window in ("session (5h)", "session", "weekly (7d)", "weekly"):
            row = latest.get((provider, window))
            if row:
                claude_rows.append((window, row))
    if claude_rows:
        session = next((row for window, row in claude_rows if "session" in window), None)
        weekly = next((row for window, row in claude_rows if "weekly" in window), None)
        result["claude"] = {
            "provider": "claude",
            "session_percent": quota_used_percent(session.get("remaining_percentage")) if session else None,
            "weekly_percent": quota_used_percent(weekly.get("remaining_percentage")) if weekly else None,
            "session_reset_minutes": reset_minutes_from_iso(session.get("next_reset_at")) if session else None,
            "weekly_reset_minutes": reset_minutes_from_iso(weekly.get("next_reset_at")) if weekly else None,
            "status": "omniroute",
            "ok": True,
        }
        _attach_today_metrics(result["claude"], "claude", metrics or {}, claude_providers)

    # OmniRoute exposes Codex/ChatGPT as a rolling session. The device's GPT
    # card intentionally presents that value as its single weekly window.
    gpt_providers = _providers_from_config(settings.get("gpt_providers"), ["codex"])
    gpt_row = next(
        (latest[(provider, window)] for provider in gpt_providers
         for window in ("weekly", "weekly (7d)", "session")
         if (provider, window) in latest),
        None,
    )
    if gpt_row:
        result["chatgpt"] = {
            "provider": "chatgpt",
            "session_percent": None,
            "weekly_percent": quota_used_percent(gpt_row.get("remaining_percentage")),
            "session_reset_minutes": None,
            "weekly_reset_minutes": reset_minutes_from_iso(gpt_row.get("next_reset_at")),
            "status": "omniroute",
            "ok": True,
        }
        _attach_today_metrics(result["chatgpt"], "chatgpt", metrics or {}, gpt_providers + ["chatgpt-web"])

    gemini_providers = _providers_from_config(settings.get("gemini_providers"), ["agy", "antigravity"])
    weekly_rows = [
        row for provider in gemini_providers
        if (row_provider := latest.get((provider, "gemini_weekly")))
        for row in [row_provider]
    ]
    model_name = str(settings.get("gemini_model", "")).strip()
    model_rows = [
        row for provider in gemini_providers
        for (row_provider, window), row in latest.items()
        if row_provider == provider and window.startswith("gemini-")
        and (not model_name or window == model_name)
    ]
    if weekly_rows or model_rows:
        # Multiple OmniRoute connections can represent different projects.
        # Showing the most constrained one is safer than hiding a near limit.
        session = min(model_rows, key=lambda row: float(row.get("remaining_percentage", 100))) if model_rows else None
        weekly = min(weekly_rows, key=lambda row: float(row.get("remaining_percentage", 100))) if weekly_rows else None
        result["gemini"] = {
            "provider": "gemini",
            "session_percent": quota_used_percent(session.get("remaining_percentage")) if session else None,
            "weekly_percent": quota_used_percent(weekly.get("remaining_percentage")) if weekly else None,
            "session_reset_minutes": reset_minutes_from_iso(session.get("next_reset_at")) if session else None,
            "weekly_reset_minutes": reset_minutes_from_iso(weekly.get("next_reset_at")) if weekly else None,
            "status": "omniroute",
            "ok": True,
        }
        _attach_today_metrics(result["gemini"], "gemini", metrics or {}, gemini_providers + ["gemini"])
    return result


def omniroute_snapshots(settings: Mapping[str, Any]) -> Dict[str, Dict[str, Any]]:
    database = Path(str(settings.get("database", "~/.omniroute/storage.sqlite"))).expanduser()
    if not database.exists():
        raise AgentError("cache SQLite do OmniRoute não encontrado: %s" % database)
    max_age = max(60, int(settings.get("max_age_seconds", 600) or 600))
    modifier = "-%d seconds" % max_age
    try:
        uri = "file:%s?mode=ro" % str(database).replace("%", "%25").replace("?", "%3F")
        connection = sqlite3.connect(uri, uri=True)
        connection.row_factory = sqlite3.Row
        rows = [dict(row) for row in connection.execute(
            "SELECT provider, window_key, remaining_percentage, next_reset_at, created_at "
            "FROM quota_snapshots WHERE created_at >= datetime('now', ?) "
            "ORDER BY created_at DESC",
            (modifier,),
        )]
        metrics = {
            str(provider): {"tokens": int(tokens or 0), "requests": int(requests or 0)}
            for provider, tokens, requests in connection.execute(
                "SELECT provider, COALESCE(SUM(tokens_input + tokens_output), 0), COUNT(*) "
                "FROM usage_history WHERE timestamp >= date('now') AND success = 1 GROUP BY provider"
            )
        }
        configured_sources = {"codex", "chatgpt-web", "gemini"}
        for key in ("claude_providers", "gpt_providers", "gemini_providers"):
            values = settings.get(key, [])
            if isinstance(values, list):
                configured_sources.update(str(value) for value in values)
        for provider in configured_sources:
            metrics.setdefault(provider, {"tokens": 0, "requests": 0})
        connection.close()
    except (OSError, sqlite3.Error) as exc:
        raise AgentError("falha ao ler cache do OmniRoute: %s" % exc)
    snapshots = build_omniroute_snapshots(rows, settings, metrics)
    if not snapshots:
        raise AgentError("OmniRoute não tem quotas recentes no cache")
    return snapshots


def claude_snapshot(provider: Mapping[str, Any]) -> Dict[str, Any]:
    raw_token = secret_for(provider)
    token = extract_access_token(raw_token or "")
    if not token:
        raise AgentError("token Claude não encontrado no Keychain")

    model = str(provider.get("model", DEFAULT_CONFIG["providers"]["claude"]["model"]))
    response = http_json(
        "https://api.anthropic.com/v1/messages",
        method="POST",
        headers={
            "Authorization": "Bearer " + token,
            "anthropic-version": "2023-06-01",
            "anthropic-beta": "oauth-2025-04-20",
            "User-Agent": "esp-dashboard-usage-agent/1",
        },
        body={"model": model, "max_tokens": 1, "messages": [{"role": "user", "content": "hi"}]},
    )
    headers = response.get("_headers", {})
    now = time.time()
    five_hour = header(headers, "anthropic-ratelimit-unified-5h-utilization")
    if five_hour:
        return {
            "provider": "claude",
            "session_percent": percent(five_hour),
            "weekly_percent": percent(header(headers, "anthropic-ratelimit-unified-7d-utilization")),
            "session_reset_minutes": reset_minutes(header(headers, "anthropic-ratelimit-unified-5h-reset"), now),
            "weekly_reset_minutes": reset_minutes(header(headers, "anthropic-ratelimit-unified-7d-reset"), now),
            "status": header(headers, "anthropic-ratelimit-unified-5h-status", "allowed"),
            "ok": True,
        }
    return {
        "provider": "claude",
        "session_percent": percent(header(headers, "anthropic-ratelimit-unified-overage-utilization")),
        "weekly_percent": 0,
        "session_reset_minutes": reset_minutes(header(headers, "anthropic-ratelimit-unified-overage-reset"), now),
        "weekly_reset_minutes": 0,
        "status": header(headers, "anthropic-ratelimit-unified-status", "allowed"),
        "ok": True,
    }


def sum_openai_results(data: Mapping[str, Any]) -> Dict[str, int]:
    tokens = 0
    requests = 0
    for bucket in data.get("data", []):
        for result in bucket.get("results", []):
            tokens += int(result.get("input_tokens", 0) or 0)
            tokens += int(result.get("output_tokens", 0) or 0)
            requests += int(result.get("num_model_requests", 0) or 0)
    return {"tokens": tokens, "requests": requests}


def openai_snapshot(provider: Mapping[str, Any]) -> Dict[str, Any]:
    key = secret_for(provider, str(provider.get("admin_key_env", "OPENAI_ADMIN_KEY")))
    if not key:
        raise AgentError("OPENAI_ADMIN_KEY não encontrado no ambiente/Keychain")
    now = int(time.time())
    start = now - 7 * 24 * 60 * 60
    query = urlencode({"start_time": start, "end_time": now, "bucket_width": "1d", "limit": 7})
    data = http_json(
        "https://api.openai.com/v1/organization/usage/completions?" + query,
        headers={"Authorization": "Bearer " + key},
    )
    totals = sum_openai_results(data)
    weekly_limit = int(provider.get("weekly_token_limit", 0) or 0)
    weekly_percent = int(round(totals["tokens"] * 100 / weekly_limit)) if weekly_limit else 0
    return {
        "provider": "chatgpt",
        "session_percent": 0,
        "weekly_percent": max(0, min(100, weekly_percent)),
        "tokens": str(totals["tokens"]),
        "requests": str(totals["requests"]),
        "status": "usage",
        "ok": True,
    }


def google_access_token(provider: Mapping[str, Any]) -> Optional[str]:
    env_name = str(provider.get("access_token_env", "GOOGLE_OAUTH_ACCESS_TOKEN"))
    if os.environ.get(env_name):
        return os.environ[env_name]

    token = read_keychain(
        str(provider.get("access_token_keychain_service", "ESP Dashboard Google OAuth Token")),
        str(provider.get("keychain_account", getpass.getuser())),
    )
    if token:
        return token

    adc_path = Path(str(provider.get("adc_file", "~/.config/gcloud/application_default_credentials.json"))).expanduser()
    try:
        adc = json.loads(adc_path.read_text())
    except (OSError, ValueError):
        return None
    if adc.get("type") != "authorized_user":
        return None
    refresh_token = adc.get("refresh_token")
    client_id = adc.get("client_id")
    client_secret = adc.get("client_secret")
    if not all(isinstance(value, str) and value for value in (refresh_token, client_id, client_secret)):
        return None
    response = http_form(
        "https://oauth2.googleapis.com/token",
        {
            "client_id": client_id,
            "client_secret": client_secret,
            "refresh_token": refresh_token,
            "grant_type": "refresh_token",
        },
    )
    access_token = response.get("access_token")
    return access_token if isinstance(access_token, str) else None


def monitoring_total(data: Mapping[str, Any]) -> float:
    total = 0.0
    for series in data.get("timeSeries", []):
        for point in series.get("points", []):
            value = point.get("value", {})
            if "int64Value" in value:
                total += float(value["int64Value"])
            elif "doubleValue" in value:
                total += float(value["doubleValue"])
    return total


def monitoring_metric(project_id: str, access_token: str, metric: str,
                      hours: int) -> Mapping[str, Any]:
    end = time.time()
    start = end - max(1, hours) * 60 * 60
    query = urlencode({
        "filter": 'metric.type = "%s"' % metric,
        "interval.startTime": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(start)),
        "interval.endTime": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(end)),
        "aggregation.alignmentPeriod": "86400s",
        "aggregation.perSeriesAligner": "ALIGN_SUM",
        "pageSize": "1000",
    })
    return http_json(
        "https://monitoring.googleapis.com/v3/projects/%s/timeSeries?%s" % (project_id, query),
        headers={"Authorization": "Bearer " + access_token},
    )


def gemini_snapshot(provider: Mapping[str, Any]) -> Dict[str, Any]:
    snapshot_file = str(provider.get("snapshot_file", "")).strip()
    if snapshot_file:
        path = Path(snapshot_file).expanduser()
        try:
            data = json.loads(path.read_text())
        except (OSError, ValueError) as exc:
            raise AgentError("snapshot Gemini inválido: %s" % exc)
        if not isinstance(data, Mapping):
            raise AgentError("snapshot Gemini precisa ser um objeto JSON")
        return {"provider": "gemini", **dict(data), "ok": bool(data.get("ok", True))}

    project_id = str(provider.get("project_id", "")).strip()
    access_token = google_access_token(provider)
    if not project_id or not access_token:
        raise AgentError("Gemini requer project_id e OAuth do Google Cloud")

    usage_metric = str(provider.get(
        "usage_metric",
        "generativelanguage.googleapis.com/quota/generate_requests_per_model/usage",
    ))
    limit_metric = str(provider.get(
        "limit_metric",
        "generativelanguage.googleapis.com/quota/generate_requests_per_model/limit",
    ))
    hours = int(provider.get("window_hours", 24) or 24)
    usage = monitoring_total(monitoring_metric(project_id, access_token, usage_metric, hours))
    limit = monitoring_total(monitoring_metric(project_id, access_token, limit_metric, hours))
    request_limit = int(provider.get("requests_limit", 0) or 0)
    effective_limit = request_limit or int(limit)
    usage_percent = int(round(usage * 100 / effective_limit)) if effective_limit else 0
    return {
        "provider": "gemini",
        "session_percent": max(0, min(100, usage_percent)),
        "weekly_percent": 0,
        "requests": str(int(round(usage))),
        "status": "project-quota",
        "ok": True,
    }


def collect(provider_name: str, provider: Mapping[str, Any]) -> Dict[str, Any]:
    if provider_name == "claude":
        return claude_snapshot(provider)
    if provider_name == "openai":
        return openai_snapshot(provider)
    if provider_name == "gemini":
        return gemini_snapshot(provider)
    raise AgentError("provedor não suportado: " + provider_name)


def post_snapshot(device_url: str, snapshot: Mapping[str, Any]) -> None:
    base = device_url.rstrip("/")
    http_json(base + "/api/usage", method="POST", body=snapshot, timeout=10)


def run_once(config: Mapping[str, Any]) -> None:
    device_url = str(config.get("device_url", "")).strip()
    if not device_url:
        raise AgentError("device_url não configurado")
    providers = config.get("providers", {})
    updated_from_omniroute = set()
    omniroute = providers.get("omniroute", {}) if isinstance(providers, Mapping) else {}
    if isinstance(omniroute, Mapping) and omniroute.get("enabled", False):
        try:
            snapshots = omniroute_snapshots(omniroute)
            for provider_name, snapshot in snapshots.items():
                post_snapshot(device_url, snapshot)
                updated_from_omniroute.add(provider_name)
                log("%s atualizado via OmniRoute" % provider_name)
        except AgentError as exc:
            log("omniroute indisponível: %s" % exc)
    for name, provider in providers.items():
        if not isinstance(provider, Mapping) or not provider.get("enabled", False):
            continue
        if name == "omniroute" or name in updated_from_omniroute:
            continue
        try:
            snapshot = collect(name, provider)
            post_snapshot(device_url, snapshot)
            log("%s atualizado no ESP" % name)
        except AgentError as exc:
            log("%s indisponível: %s" % (name, exc))


def install_launch_agent(script_path: Path) -> Path:
    plist_path = Path.home() / "Library" / "LaunchAgents" / (APP_LABEL + ".plist")
    plist_path.parent.mkdir(parents=True, exist_ok=True)
    log_file = log_path()
    log_file.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "Label": APP_LABEL,
        "ProgramArguments": [sys.executable, str(script_path), "--run"],
        "WorkingDirectory": str(script_path.parent.parent.parent),
        "RunAtLoad": True,
        "KeepAlive": True,
        "ProcessType": "Background",
        "LowPriorityIO": True,
        "StandardOutPath": str(log_file),
        "StandardErrorPath": str(log_file),
    }
    plist_path.write_bytes(plistlib.dumps(payload))
    os.chmod(plist_path, 0o600)
    uid = str(os.getuid())
    subprocess.run(["launchctl", "bootout", "gui/" + uid + "/" + APP_LABEL], check=False)
    result = subprocess.run(["launchctl", "bootstrap", "gui/" + uid, str(plist_path)], check=False)
    if result.returncode != 0:
        raise AgentError("launchctl bootstrap falhou; plist salvo em %s" % plist_path)
    return plist_path


def uninstall_launch_agent() -> None:
    uid = str(os.getuid())
    subprocess.run(["launchctl", "bootout", "gui/" + uid + "/" + APP_LABEL], check=False)
    plist_path = Path.home() / "Library" / "LaunchAgents" / (APP_LABEL + ".plist")
    plist_path.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, help="caminho alternativo do config.json")
    parser.add_argument("--device-url", help="URL do ESP para inicializar a configuração")
    parser.add_argument("--init", action="store_true", help="cria/atualiza configuração local")
    parser.add_argument("--install", action="store_true", help="instala o agente como serviço do macOS")
    parser.add_argument("--uninstall", action="store_true", help="remove o serviço do macOS")
    parser.add_argument("--once", action="store_true", help="executa uma coleta e encerra")
    parser.add_argument("--run", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()

    if args.uninstall:
        uninstall_launch_agent()
        log("agente removido")
        return 0

    path = args.config or config_path()
    config = load_config(path)
    if args.device_url:
        config["device_url"] = args.device_url.rstrip("/")
    if args.init or args.device_url:
        save_config(config, path)
        log("configuração salva em %s" % path)
    if args.install:
        if sys.platform != "darwin":
            raise AgentError("--install só é suportado no macOS")
        plist = install_launch_agent(Path(__file__).resolve())
        log("agente instalado em %s" % plist)
        return 0
    if args.once:
        run_once(config)
        return 0
    while True:
        run_once(config)
        time.sleep(max(30, int(config.get("interval_seconds", DEFAULT_INTERVAL))))


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AgentError as exc:
        log("erro: %s" % exc)
        raise SystemExit(1)
