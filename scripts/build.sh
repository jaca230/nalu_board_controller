#!/bin/bash

# Get the absolute path of the script directory
SCRIPT_DIR=$(dirname "$(realpath "$0")")

# Default overwrite flag is false
OVERWRITE=false

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -o|--overwrite) OVERWRITE=true; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Build directory (relative to the script directory)
BUILD_DIR="$SCRIPT_DIR/../build"

# If overwrite flag is set, remove the build directory
if [ "$OVERWRITE" = true ]; then
    echo "Overwrite flag set: Cleaning previous build..."
    rm -rf "$BUILD_DIR"
fi

# Create the build directory if it doesn't exist
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake to configure the project
echo "Configuring the project with CMake..."
cmake "$SCRIPT_DIR/.."

# Build the project
echo "Building the project..."
make

# Show the final binary and library
echo "Build finished! Executable and libraries are in the bin/ and lib/ directories."
