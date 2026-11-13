#!/bin/bash
set -euo pipefail
SCRIPT_DIR=$(dirname "$(realpath "$0")")
APP_DIR=$(realpath "$SCRIPT_DIR/..")
cmake -S "$APP_DIR" -B "$APP_DIR/build"
cmake --build "$APP_DIR/build" -j"$(nproc)"
