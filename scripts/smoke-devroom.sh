#!/usr/bin/env bash
# Non-interactive invariant check: source -> build -> playable devroom.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LITEKNIGHT_BUILD_DIR:-${ROOT}/build}"
LOG="${BUILD_DIR}/devroom-smoke.log"
BIN="${BUILD_DIR}/soul-knight"

if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "ERROR: Wayland display is required for the devroom smoke test." >&2
    exit 2
fi
if [[ ! -x "${BIN}" ]]; then
    echo "ERROR: devroom executable missing: ${BIN}" >&2
    exit 2
fi
"${ROOT}/scripts/prepare-assets.sh" >/dev/null

set +e
"${BIN}" --devroom --smoke-seconds 5 2>&1 | tee "${LOG}"
STATUS=${PIPESTATUS[0]}
set -e
if [[ ${STATUS} -ne 0 ]]; then
    echo "ERROR: devroom smoke exited ${STATUS}" >&2
    exit "${STATUS}"
fi
for marker in \
    "SDL video driver: wayland" \
    "Vulkan validation:" \
    "Scene loaded: r1_1_visual" \
    "Player render binding:" \
    "Render batch:" \
    "Smoke test duration reached" \
    "Shutdown complete"; do
    if ! grep -a -q -- "${marker}" "${LOG}"; then
        echo "ERROR: devroom smoke is missing: ${marker}" >&2
        exit 1
    fi
done
if grep -a -q -E 'Vulkan validation: (vk|VUID)|LeakSanitizer|AddressSanitizer|ERROR \[RENDER\]' "${LOG}"; then
    echo "ERROR: sanitizer/validation/renderer failure in ${LOG}" >&2
    exit 1
fi
echo "LiteKnight devroom smoke: PASS"
