#!/usr/bin/env bash
# Run the deterministic native test suite for a build tree.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LITEKNIGHT_BUILD_DIR:-${ROOT}/build}"

if [[ ! -f "${BUILD_DIR}/CTestTestfile.cmake" ]]; then
    echo "ERROR: test build missing: ${BUILD_DIR}" >&2
    echo "Run scripts/build-dev.sh first." >&2
    exit 2
fi
ctest --test-dir "${BUILD_DIR}" --output-on-failure "$@"
