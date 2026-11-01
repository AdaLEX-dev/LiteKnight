#!/usr/bin/env bash
# Manual 30-second visual review harness. It launches one real game state, lets
# the reviewer explore, then freezes and waits for an explicit terminal ENTER.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LITEKNIGHT_BUILD_DIR:-${ROOT}/build}"
BIN="${BUILD_DIR}/soul-knight"
AREA="${1:-main_menu}"
SECONDS_TO_REVIEW="${LITEKNIGHT_REVIEW_SECONDS:-30}"

if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "ERROR: Wayland display is required for the visual review test." >&2
    exit 2
fi
if [[ ! -x "${BIN}" ]]; then
    echo "ERROR: executable missing: ${BIN}" >&2
    echo "Run ./scripts/build-dev.sh first." >&2
    exit 2
fi
"${ROOT}/scripts/prepare-assets.sh" >/dev/null

echo "============================================================"
echo "                30 SECOND REVIEW TEST"
echo "============================================================"
echo "AREA: ${AREA}"
echo
echo "The application will run for ${SECONDS_TO_REVIEW} seconds."
echo "Explore the area and inspect the visual state."
echo
echo "After the timer the application will PAUSE and wait for review."
echo "It will NOT continue until you press ENTER in this terminal."
echo "============================================================"

export LITEKNIGHT_PROJECT_ROOT="${ROOT}"
export LITEKNIGHT_REFERENCE_APK="${ROOT}/original/SoulKnight-2.6.8.apk"
exec "${BIN}" --review-area "${AREA}" --review-seconds "${SECONDS_TO_REVIEW}"
