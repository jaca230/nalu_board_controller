#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
APP_DIR=$(realpath "$SCRIPT_DIR/..")
PROJECT_DIR=$(realpath "$APP_DIR/../../..")
BUILD_DIR="$APP_DIR/build"
OVERWRITE=false
USE_LOCAL_LIBRARY=true
LIBRARY_SOURCE_DIR="$PROJECT_DIR"

print_help() {
    cat <<EOF
Usage: ./apps/examples/manual_capture/scripts/build.sh [options]

Build the manual_capture app.

Options:
  -o, --overwrite   Clean the build directory before building
  --installed       Build against an installed nalu_board_controller package
  --library-source PATH
                    Path to a local nalu_board_controller source tree
  -h, --help        Show this help message
EOF
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -o|--overwrite) OVERWRITE=true; shift ;;
        --installed) USE_LOCAL_LIBRARY=false; shift ;;
        --library-source) LIBRARY_SOURCE_DIR="$2"; shift 2 ;;
        -h|--help) print_help; exit 0 ;;
        *) echo "Unknown option: $1" >&2; echo; print_help; exit 1 ;;
    esac
done

if [ "$OVERWRITE" = true ]; then
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

CMAKE_ARGS=()
if [ "$USE_LOCAL_LIBRARY" = true ]; then
    CMAKE_ARGS+=(
        -DUSE_LOCAL_NALU_BOARD_CONTROLLER=ON
        -DNALU_BOARD_CONTROLLER_SOURCE_DIR="$LIBRARY_SOURCE_DIR"
    )
else
    CMAKE_ARGS+=(-DUSE_LOCAL_NALU_BOARD_CONTROLLER=OFF)
fi

cmake -S "$APP_DIR" -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --parallel
