#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
APP_DIR=$(realpath "$SCRIPT_DIR/../..")
RUNNER="$APP_DIR/scripts/run.sh"
BUILDER="$APP_DIR/scripts/build.sh"
SESSION_NAME="packet_listener"
CONFIG_PATH="$APP_DIR/config.json"
BUILD=false
OVERWRITE=false
SAMPLE_COUNT=""

print_help() {
    cat <<EOF
Usage: ./apps/examples/packet_listener/scripts/screening/start.sh [options]

Start packet_listener in a detached screen session.

Options:
  --session NAME    Screen session name (default: packet_listener)
  --config PATH     Path to config.json
  --sample-count N  Stop after parsing N packets and print a summary
  --build           Build apps before starting
  --overwrite       When used with --build, clean the build directory first
  -h, --help        Show this help message
EOF
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --session) SESSION_NAME="$2"; shift 2 ;;
        --config) CONFIG_PATH="$2"; shift 2 ;;
        --sample-count) SAMPLE_COUNT="$2"; shift 2 ;;
        --build) BUILD=true; shift ;;
        --overwrite) OVERWRITE=true; shift ;;
        -h|--help) print_help; exit 0 ;;
        *) echo "Unknown option: $1" >&2; echo; print_help; exit 1 ;;
    esac
done

RUN_ARGS=(--config "$CONFIG_PATH")
if [ -n "$SAMPLE_COUNT" ]; then
    RUN_ARGS+=(--sample-count "$SAMPLE_COUNT")
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
