#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LITEKNIGHT_BUILD_DIR:-${ROOT}/build}"
cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "${BUILD_DIR}" --target liteknight-texture-gallery
echo "Gallery: ${BUILD_DIR}/bin/liteknight-texture-gallery"
