#!/usr/bin/env bash
# Build the project
# Usage: ./tools/build.sh [Debug|RelWithDebInfo|Release] [target]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"

CONFIG="${1:-Debug}"
TARGET="${2:-}"

echo "=== Soul Knight Linux Build ==="
echo "Config: ${CONFIG}"
if [ -n "${TARGET}" ]; then
    echo "Target: ${TARGET}"
fi
echo

# Verify build directory exists
if [ ! -f "${BUILD_DIR}/build.ninja" ] && [ ! -f "${BUILD_DIR}/Makefile" ]; then
    echo "Build directory not configured. Run ./tools/configure.sh first"
    exit 1
fi

# Build
cd "${PROJECT_ROOT}"
if [ -n "${TARGET}" ]; then
    cmake --build "${BUILD_DIR}" --target "${TARGET}" --config "${CONFIG}" -j$(nproc)
else
    cmake --build "${BUILD_DIR}" --config "${CONFIG}" -j$(nproc)
fi

BUILD_RESULT=$?

if [ ${BUILD_RESULT} -eq 0 ]; then
    echo
    echo "=== Build Successful ==="
    if [ -f "${BUILD_DIR}/soul-knight" ]; then
        echo "Binary: ${BUILD_DIR}/soul-knight"
        ls -lh "${BUILD_DIR}/soul-knight"
    fi
else
    echo
    echo "=== Build Failed ==="
    exit ${BUILD_RESULT}
fi