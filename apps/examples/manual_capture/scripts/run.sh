#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
APP_DIR=$(realpath "$SCRIPT_DIR/..")
EXECUTABLE="$APP_DIR/build/bin/manual_capture"
CONFIG_PATH="$APP_DIR/config.json"
DEBUG=false
VENV_PATH="$APP_DIR/.venv"
USE_SYSTEM_PYTHON=false
SETUP_VENV=false
SETUP_VENV_SCRIPT="$APP_DIR/scripts/environment/setup_venv.sh"

prepare_python_env() {
    if [ "$USE_SYSTEM_PYTHON" = true ]; then
        export PYTHONNOUSERSITE=1
        return
    fi

    if [ "$SETUP_VENV" = true ]; then
        "$SETUP_VENV_SCRIPT" --venv "$VENV_PATH"
    fi

    if [ ! -x "$VENV_PATH/bin/python" ]; then
        echo "Virtual environment not found: $VENV_PATH" >&2
        echo "Create it with ./apps/examples/manual_capture/scripts/environment/setup_venv.sh or run with --setup-venv." >&2
        exit 1
    fi

    local site_packages
    site_packages=$("$VENV_PATH/bin/python" - <<'PY'
import site
paths = [path for path in site.getsitepackages() if path.endswith("site-packages")]
if not paths:
    raise SystemExit("No site-packages directory found in virtual environment")
print(paths[0])
PY
)

    export VIRTUAL_ENV="$VENV_PATH"
    export PATH="$VENV_PATH/bin:$PATH"
    export PYTHONNOUSERSITE=1
    export PYTHONPATH="$site_packages${PYTHONPATH:+:$PYTHONPATH}"
    unset PYTHONHOME
}

print_help() {
    cat <<EOF
Usage: ./apps/examples/manual_capture/scripts/run.sh [options]

Run the manual capture app.

Options:
  --config PATH     Path to config.json (default: apps/examples/manual_capture/config.json)
  --venv PATH       Virtual environment path (default: apps/examples/manual_capture/.venv)
  --setup-venv      Create/update the virtual environment before running
  --system-python   Skip the app-local virtual environment and use the system Python setup
  --debug           Run under gdb
  -h, --help        Show this help message

Build first with:
  ./apps/examples/manual_capture/scripts/build.sh

Set up Python first with:
  ./apps/examples/manual_capture/scripts/environment/setup_venv.sh
EOF
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --config) CONFIG_PATH="$2"; shift 2 ;;
        --venv) VENV_PATH="$2"; shift 2 ;;
        --setup-venv) SETUP_VENV=true; shift ;;
        --system-python) USE_SYSTEM_PYTHON=true; shift ;;
        --debug) DEBUG=true; shift ;;
        -h|--help) print_help; exit 0 ;;
        *) echo "Unknown option: $1" >&2; echo; print_help; exit 1 ;;
    esac
done

if [ ! -f "$EXECUTABLE" ]; then
    echo "Executable not found: $EXECUTABLE" >&2
    echo "Build it with ./apps/examples/manual_capture/scripts/build.sh." >&2
    exit 1
fi

if [ ! -f "$CONFIG_PATH" ]; then
    echo "Config file not found: $CONFIG_PATH" >&2
    exit 1
fi

prepare_python_env

if [ "$DEBUG" = true ]; then
    exec gdb --args "$EXECUTABLE" --config "$CONFIG_PATH"
else
    exec "$EXECUTABLE" --config "$CONFIG_PATH"
fi
