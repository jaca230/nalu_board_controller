#!/bin/bash
set -euo pipefail
SCRIPT_DIR=$(dirname "$(realpath "$0")")
APP_DIR=$(realpath "$SCRIPT_DIR/..")
VENV="$APP_DIR/../manual_capture/.venv"
SCREEN_NAME="firmware_959_rate_scan"
SCREEN_MODE=false
SCREEN_STOP=false
SCREEN_STATUS=false
SCAN_ARGS=()

while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --screen) SCREEN_MODE=true; shift ;;
        --screen-name) SCREEN_NAME="$2"; shift 2 ;;
        --screen-stop) SCREEN_STOP=true; shift ;;
        --screen-status) SCREEN_STATUS=true; shift ;;
        -h|--help)
            cat <<EOF
Usage: $0 [--screen] [--screen-name NAME] [--screen-stop|--screen-status] [scan options]

Scan options such as --config and --dry-run are passed to run_scan.py.
EOF
            exit 0 ;;
        *) SCAN_ARGS+=("$1"); shift ;;
    esac
done

if "$SCREEN_STATUS"; then
    screen -ls | grep -F "$SCREEN_NAME" || true
    exit 0
fi
if "$SCREEN_STOP"; then
    screen -S "$SCREEN_NAME" -X stuff $'\003'
    exit 0
fi

export VIRTUAL_ENV="$VENV"
export PATH="$VENV/bin:$PATH"
export PYTHONNOUSERSITE=1
export PYTHONPATH="$("$VENV/bin/python" -c 'import site; print(site.getsitepackages()[0])')${PYTHONPATH:+:$PYTHONPATH}"

if "$SCREEN_MODE"; then
    command -v screen >/dev/null || { echo "screen is not installed" >&2; exit 1; }
    mkdir -p "$APP_DIR/rate_data"
    LOG_PATH="$APP_DIR/rate_data/${SCREEN_NAME}.screen.log"
    printf -v SCREEN_COMMAND '%q ' "$VENV/bin/python" "$SCRIPT_DIR/run_scan.py" "${SCAN_ARGS[@]}"
    screen -L -Logfile "$LOG_PATH" -dmS "$SCREEN_NAME" bash -lc "exec $SCREEN_COMMAND"
    echo "Started screen session '$SCREEN_NAME'"
    echo "Attach: screen -r $SCREEN_NAME"
    echo "Log: $LOG_PATH"
    exit 0
fi

exec "$VENV/bin/python" "$SCRIPT_DIR/run_scan.py" "${SCAN_ARGS[@]}"
