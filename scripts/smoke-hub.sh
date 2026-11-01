#!/usr/bin/env bash
# Non-interactive Living Room smoke: default hierarchy, cleanup, collision audit.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LITEKNIGHT_BUILD_DIR:-${ROOT}/build}"
LOG="${BUILD_DIR}/hub-smoke.log"
BIN="${BUILD_DIR}/soul-knight"
if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "ERROR: Wayland display is required for the hub smoke test." >&2
    exit 2
fi
"${ROOT}/scripts/prepare-assets.sh" >/dev/null
python3 "${ROOT}/tools/asset/audit_hub_topology.py"
set +e
"${BIN}" --hub --smoke-seconds 4 2>&1 | tee "${LOG}"
STATUS=${PIPESTATUS[0]}
set -e
[[ ${STATUS} -eq 0 ]] || { echo "ERROR: hub smoke exited ${STATUS}" >&2; exit "${STATUS}"; }
for marker in "Scene loaded: hero_room" "Smoke test duration reached" "Shutdown complete"; do
    grep -a -q -- "${marker}" "${LOG}" || { echo "ERROR: hub smoke missing ${marker}" >&2; exit 1; }
done
if grep -a -q -E 'AddressSanitizer|LeakSanitizer|ERROR \[RENDER\]|FATAL' "${LOG}"; then
    echo "ERROR: hub smoke failure in ${LOG}" >&2; exit 1
fi
echo "LiteKnight hub smoke: PASS"
