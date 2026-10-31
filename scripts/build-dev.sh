#!/usr/bin/env bash
# Configure/build the native LiteKnight development executable.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LITEKNIGHT_BUILD_DIR:-${ROOT}/build}"
BUILD_TYPE="${LITEKNIGHT_BUILD_TYPE:-RelWithDebInfo}"
JOBS="${LITEKNIGHT_JOBS:-$(nproc)}"

# Reconfigure every time so the embedded version/commit hash and generated
# launcher always match the current checkout.
cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "${BUILD_DIR}" -j "${JOBS}" --target soul-knight soul-knight-test liteknight-devroom
"${ROOT}/scripts/prepare-assets.sh"
echo "Build complete: ${BUILD_DIR}/soul-knight"
echo "Launch:          ${BUILD_DIR}/bin/liteknight-devroom"
