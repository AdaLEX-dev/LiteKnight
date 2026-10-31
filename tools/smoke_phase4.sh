#!/usr/bin/env bash
# Phase 4 manual integration smoke test.
# Requires a real Wayland session, SDL3, Vulkan 1.3, and a working GPU.
# This is intentionally not a CTest test: CTest must not require a compositor.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build}"
BIN="${BUILD_DIR}/soul-knight"
LOG="${BUILD_DIR}/phase4-smoke.log"

if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "ERROR: WAYLAND_DISPLAY is not set; this smoke test requires Wayland." >&2
    exit 2
fi
if [[ ! -x "${BIN}" ]]; then
    echo "ERROR: ${BIN} not found. Build Debug or RelWithDebInfo first." >&2
    exit 2
fi

set +e
"${BIN}" --smoke-test "$@" 2>&1 | tee "${LOG}"
STATUS=${PIPESTATUS[0]}
set -e

if [[ ${STATUS} -ne 0 ]]; then
    echo "ERROR: smoke test exited ${STATUS}" >&2
    exit "${STATUS}"
fi

required=(
    "SDL video driver: wayland"
    "Wayland-only mode: enabled"
    "Vulkan validation:"
    "Swapchain:"
    "Sprite renderer ready"
    "Smoke test duration reached"
    "Shutdown complete"
)
for marker in "${required[@]}"; do
    if ! grep -a -q -- "${marker}" "${LOG}"; then
        echo "ERROR: smoke log is missing: ${marker}" >&2
        exit 1
    fi
done

# Debug must actually enable validation when the layer is available. Release
# builds intentionally do not depend on validation layers.
if grep -a -q 'Version: .* (Debug)' "${LOG}" && ! grep -a -q 'Vulkan validation: true' "${LOG}"; then
    echo "ERROR: Debug smoke test did not enable Vulkan validation" >&2
    exit 1
fi

if grep -a -q -E 'Vulkan validation: (vk|VUID)|LeakSanitizer|ERROR \[RENDER\]' "${LOG}"; then
    echo "ERROR: validation or renderer errors found in ${LOG}" >&2
    exit 1
fi

echo "Phase 4 Wayland/Vulkan smoke test: PASS"
