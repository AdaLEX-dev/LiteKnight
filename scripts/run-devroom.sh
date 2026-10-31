#!/usr/bin/env bash
# Launch the always-playable native LiteKnight devroom.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LITEKNIGHT_BUILD_DIR:-${ROOT}/build}"
"${ROOT}/scripts/prepare-assets.sh" >/dev/null

if [[ ! -x "${BUILD_DIR}/soul-knight" ]]; then
    echo "ERROR: devroom executable missing: ${BUILD_DIR}/soul-knight" >&2
    echo "Run scripts/build-dev.sh first." >&2
    exit 2
fi

# Wayland-only by design; do not silently fall back to X11/XWayland.
export SDL_VIDEODRIVER=wayland
if [[ -x "${BUILD_DIR}/bin/liteknight-devroom" ]]; then
    exec "${BUILD_DIR}/bin/liteknight-devroom" "$@"
fi
exec "${BUILD_DIR}/soul-knight" --devroom "$@"
