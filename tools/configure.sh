#!/usr/bin/env bash
# Configure build with CMake presets
# Usage: ./tools/configure.sh [Debug|RelWithDebInfo|Release]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"

CONFIG="${1:-Debug}"
PRESET="linux-${CONFIG,,}"

echo "=== Soul Knight Linux Build Configuration ==="
echo "Project: ${PROJECT_ROOT}"
echo "Build:   ${BUILD_DIR}"
echo "Config:  ${CONFIG}"
echo "Preset:  ${PRESET}"
echo

# Check for required tools
for tool in cmake ninja g++; do
    if ! command -v "$tool" &> /dev/null; then
        echo "ERROR: Required tool '$tool' not found"
        exit 1
    fi
done

# Check for Vulkan SDK
if ! pkg-config --exists vulkan 2>/dev/null && ! [ -d "/usr/include/vulkan" ]; then
    echo "WARNING: Vulkan headers not found via pkg-config"
    echo "  Install: sudo pacman -S vulkan-headers vulkan-validation-layers"
fi

# Check for SDL3
if ! pkg-config --exists sdl3 2>/dev/null; then
    echo "WARNING: SDL3 not found via pkg-config"
    echo "  Install: sudo pacman -S sdl3"
fi

# Create build directory
mkdir -p "${BUILD_DIR}"

# Configure with CMake preset
cd "${PROJECT_ROOT}"
if cmake --preset="${PRESET}" 2>/dev/null; then
    echo "Using CMake preset: ${PRESET}"
else
    echo "Preset not found, using manual configuration..."
    cmake -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${CONFIG}" \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_C_COMPILER=gcc \
        -G Ninja \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi

echo
echo "=== Configuration Complete ==="
echo "Build with: ./tools/build.sh ${CONFIG}"
echo "Run with:   ./tools/run.sh ${CONFIG}"