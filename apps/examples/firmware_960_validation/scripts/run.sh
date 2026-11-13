#!/bin/bash
set -euo pipefail
SCRIPT_DIR=$(dirname "$(realpath "$0")")
APP_DIR=$(realpath "$SCRIPT_DIR/..")
EXECUTABLE="$APP_DIR/build/bin/firmware_960_validation"
VENV="$APP_DIR/../manual_capture/.venv"

if [ ! -x "$EXECUTABLE" ]; then
    echo "Build first: $APP_DIR/scripts/build.sh" >&2
    exit 1
fi
if [ ! -x "$VENV/bin/python" ]; then
    echo "Manual-capture virtual environment not found: $VENV" >&2
    exit 1
fi

export VIRTUAL_ENV="$VENV"
export PATH="$VENV/bin:$PATH"
export PYTHONNOUSERSITE=1
export PYTHONPATH="$("$VENV/bin/python" -c 'import site; print(site.getsitepackages()[0])')${PYTHONPATH:+:$PYTHONPATH}"

if [[ " ${*} " == *" --firmware-only "* ]]; then
    exec "$EXECUTABLE" --config "$APP_DIR/config.json" "$@"
fi

exec "$VENV/bin/python" "$SCRIPT_DIR/run_suite.py" "$@"
