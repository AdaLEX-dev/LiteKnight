#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LITEKNIGHT_BUILD_DIR:-${ROOT}/build}"
exec "${BUILD_DIR}/bin/liteknight-texture-gallery" "$@"
