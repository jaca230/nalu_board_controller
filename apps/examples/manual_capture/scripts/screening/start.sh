#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
APP_DIR=$(realpath "$SCRIPT_DIR/../..")
RUNNER="$APP_DIR/scripts/run.sh"
BUILDER="$APP_DIR/scripts/build.sh"
SESSION_NAME="manual_capture"
CONFIG_PATH="$APP_DIR/config.json"
VENV_PATH="$APP_DIR/.venv"
BUILD=false
OVERWRITE=false
SETUP_VENV=false
USE_SYSTEM_PYTHON=false

print_help() {
    cat <<EOF
Usage: ./apps/examples/manual_capture/scripts/screening/start.sh [options]

Start manual_capture in a detached screen session.

Options:
  --session NAME    Screen session name (default: manual_capture)
  --config PATH     Path to config.json
  --venv PATH       Virtual environment path (default: apps/examples/manual_capture/.venv)
  --build           Build apps before starting
  --overwrite       When used with --build, clean the build directory first
  --setup-venv      Create/update the virtual environment before starting
  --system-python   Skip the app-local virtual environment
  -h, --help        Show this help message
EOF
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --session) SESSION_NAME="$2"; shift 2 ;;
        --config) CONFIG_PATH="$2"; shift 2 ;;
        --venv) VENV_PATH="$2"; shift 2 ;;
        --build) BUILD=true; shift ;;
        --overwrite) OVERWRITE=true; shift ;;
        --setup-venv) SETUP_VENV=true; shift ;;
        --system-python) USE_SYSTEM_PYTHON=true; shift ;;
        -h|--help) print_help; exit 0 ;;
        *) echo "Unknown option: $1" >&2; echo; print_help; exit 1 ;;
    esac
done

RUN_ARGS=(--config "$CONFIG_PATH")
if [ "$SETUP_VENV" = true ]; then
    RUN_ARGS+=(--setup-venv)
fi
if [ "$USE_SYSTEM_PYTHON" = true ]; then
    RUN_ARGS+=(--system-python)
else
    RUN_ARGS+=(--venv "$VENV_PATH")
fi
if [ "$BUILD" = true ]; then
    BUILD_ARGS=()
    if [ "$OVERWRITE" = true ]; then
        BUILD_ARGS+=(--overwrite)
    fi
    "$BUILDER" "${BUILD_ARGS[@]}"
fi

screen -S "$SESSION_NAME" -X quit >/dev/null 2>&1 || true
screen -dmS "$SESSION_NAME" "$RUNNER" "${RUN_ARGS[@]}"

echo "Screen session '$SESSION_NAME' started."
