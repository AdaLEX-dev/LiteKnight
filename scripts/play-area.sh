#!/usr/bin/env bash
# Launch a direct developer area and run normally until the window is closed.
# This replaces the retired timed 30-second review harness.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LITEKNIGHT_BUILD_DIR:-${ROOT}/build}"
AREA="${1:-main_menu}"
"${ROOT}/scripts/prepare-assets.sh" >/dev/null
if [[ ! -x "${BUILD_DIR}/soul-knight" ]]; then
    echo "ERROR: executable missing: ${BUILD_DIR}/soul-knight" >&2
    exit 2
fi
export SDL_VIDEODRIVER=wayland
export LITEKNIGHT_PROJECT_ROOT="${ROOT}"
export LITEKNIGHT_REFERENCE_APK="${ROOT}/original/SoulKnight-2.6.8.apk"
case "${AREA}" in
    main_menu) exec "${BUILD_DIR}/soul-knight" --start-gui "${@:2}" ;;
    living_room|workshop|garden|cellar|trader|chest|weapons|devroom)
        exec "${BUILD_DIR}/soul-knight" --area "${AREA}" "${@:2}" ;;
    r1_1|r1_2|r1_3)
        exec "${BUILD_DIR}/soul-knight" --scene "scenes/${AREA}_visual/scene.json" "${@:2}" ;;
    *)
        echo "ERROR: unknown area: ${AREA}" >&2
        echo "Areas: main_menu living_room workshop garden cellar trader chest weapons devroom r1_1 r1_2 r1_3" >&2
        exit 2 ;;
esac
