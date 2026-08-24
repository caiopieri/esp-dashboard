#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PYTHON=${PYTHON:-$(command -v python3)}

if [ "$#" -gt 1 ]; then
  echo "Uso: $0 [http://IP_DO_ESP32]" >&2
  exit 2
fi

if [ "$#" -eq 1 ]; then
  exec "$PYTHON" "$SCRIPT_DIR/usage_agent.py" --device-url "$1" --init --install
fi
exec "$PYTHON" "$SCRIPT_DIR/usage_agent.py" --install
