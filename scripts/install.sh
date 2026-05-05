#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/..")

INSTALL_PREFIX="/usr/local"
OVERWRITE=false
BUILD_APPS=OFF

print_help() {
    cat <<EOF
Usage: ./scripts/install.sh [options]

Configure, build, and install the project.

Options:
  -p, --prefix DIR  Install prefix (default: /usr/local)
  -o, --overwrite   Remove the existing build directory before configuring
  --apps            Build opt-in apps in addition to the library
  -h, --help        Show this help message
EOF
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -o|--overwrite) OVERWRITE=true; shift ;;
        -p|--prefix) INSTALL_PREFIX="$2"; shift 2 ;;
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
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DBUILD_APPS="$BUILD_APPS"

echo "Building the project..."
cmake --build "$BUILD_DIR" --parallel

echo "Installing the project to $INSTALL_PREFIX..."
sudo cmake --install "$BUILD_DIR"

echo "Installation finished!"
echo "Headers and libraries are installed in $INSTALL_PREFIX/include and $INSTALL_PREFIX/lib."
