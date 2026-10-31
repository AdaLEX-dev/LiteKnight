#!/usr/bin/env bash
# Launch the native player-facing flow: main menu -> Living Room.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LITEKNIGHT_BUILD_DIR:-${ROOT}/build}"
"${ROOT}/scripts/prepare-assets.sh" >/dev/null
if [[ ! -x "${BUILD_DIR}/soul-knight" ]]; then
    echo "ERROR: executable missing: ${BUILD_DIR}/soul-knight" >&2
    echo "Run scripts/build-dev.sh first." >&2
    exit 2
fi
export SDL_VIDEODRIVER=wayland
export LITEKNIGHT_PROJECT_ROOT="${ROOT}"
export LITEKNIGHT_REFERENCE_APK="${ROOT}/original/SoulKnight-2.6.8.apk"
exec "${BUILD_DIR}/soul-knight" --start-gui "$@"
