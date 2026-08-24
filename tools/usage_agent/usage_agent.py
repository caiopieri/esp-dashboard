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
            "enabled": True,
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


def gemini_snapshot(provider: Mapping[str, Any]) -> Dict[str, Any]:
    snapshot_file = str(provider.get("snapshot_file", "")).strip()
    if not snapshot_file:
        raise AgentError("Gemini requer snapshot_file ou adaptador Cloud Monitoring")
    path = Path(snapshot_file).expanduser()
    try:
        data = json.loads(path.read_text())
    except (OSError, ValueError) as exc:
        raise AgentError("snapshot Gemini inválido: %s" % exc)
    if not isinstance(data, Mapping):
        raise AgentError("snapshot Gemini precisa ser um objeto JSON")
    return {"provider": "gemini", **dict(data), "ok": bool(data.get("ok", True))}


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
    for name, provider in providers.items():
        if not isinstance(provider, Mapping) or not provider.get("enabled", False):
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
