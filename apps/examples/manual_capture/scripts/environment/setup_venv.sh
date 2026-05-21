#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
APP_DIR=$(realpath "$SCRIPT_DIR/../..")
VENV_PATH="$APP_DIR/.venv"
PYTHON_BIN="python3"
RECREATE=false

print_help() {
    cat <<EOF
Usage: ./apps/examples/manual_capture/scripts/environment/setup_venv.sh [options]

Create or update the manual_capture Python environment.

Options:
  --venv PATH       Virtual environment path (default: apps/examples/manual_capture/.venv)
  --python BIN      Python executable to use for venv creation (default: python3)
  --recreate        Delete and recreate the virtual environment
  -h, --help        Show this help message
EOF
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --venv) VENV_PATH="$2"; shift 2 ;;
        --python) PYTHON_BIN="$2"; shift 2 ;;
        --recreate) RECREATE=true; shift ;;
        -h|--help) print_help; exit 0 ;;
        *) echo "Unknown option: $1" >&2; echo; print_help; exit 1 ;;
    esac
done

if [ "$RECREATE" = true ] && [ -d "$VENV_PATH" ]; then
    rm -rf "$VENV_PATH"
fi

if [ ! -d "$VENV_PATH" ]; then
    if ! "$PYTHON_BIN" -m venv "$VENV_PATH"; then
        rm -rf "$VENV_PATH"
        cat >&2 <<EOF
Failed to create virtual environment with: $PYTHON_BIN -m venv

This host is missing the stdlib venv support. On Debian/Ubuntu, install it with:
  sudo apt install python3-venv

Then rerun:
  ./apps/examples/manual_capture/scripts/environment/setup_venv.sh

If you need to run immediately against an existing Python install, use:
  ./apps/examples/manual_capture/scripts/run.sh --system-python
EOF
        exit 1
    fi
fi

if ! "$VENV_PATH/bin/python" -m pip --version >/dev/null 2>&1; then
    cat >&2 <<EOF
The virtual environment at $VENV_PATH does not have pip available.

This usually means the host Python was built without the venv/ensurepip components.
On Debian/Ubuntu, install:
  sudo apt install python3-venv

Then recreate the environment:
  ./apps/examples/manual_capture/scripts/environment/setup_venv.sh --recreate

If you need to run immediately against an existing Python install, use:
  ./apps/examples/manual_capture/scripts/run.sh --system-python
EOF
    exit 1
fi

"$VENV_PATH/bin/python" -m pip install --upgrade pip setuptools wheel
"$VENV_PATH/bin/python" -m pip install -r "$APP_DIR/requirements.txt"

echo "manual_capture Python environment ready at: $VENV_PATH"
