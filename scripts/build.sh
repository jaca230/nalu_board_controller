#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/..")

OVERWRITE=false
BUILD_APPS=OFF

print_help() {
    cat <<EOF
Usage: ./scripts/build.sh [options]

Configure and build the project.

Options:
  -o, --overwrite   Remove the existing build directory before configuring
  --apps            Build opt-in apps in addition to the library
  -h, --help        Show this help message
EOF
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -o|--overwrite) OVERWRITE=true; shift ;;
        --apps) BUILD_APPS=ON; shift ;;
        -h|--help) print_help; exit 0 ;;
        *) echo "Unknown option: $1" >&2; echo; print_help; exit 1 ;;
    esac
done

BUILD_DIR="$SCRIPT_DIR/../build"

if [ "$OVERWRITE" = true ]; then
    echo "Overwrite flag set: Cleaning previous build..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

echo "Configuring the project with CMake..."
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DBUILD_APPS="$BUILD_APPS"

echo "Building the project..."
cmake --build "$BUILD_DIR" --parallel

echo "Build finished! Libraries are in lib/ and opt-in apps are in bin/."
