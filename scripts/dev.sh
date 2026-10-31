#!/usr/bin/env bash
# One-command build -> test -> play workflow for LiteKnight.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
DO_BUILD=1
DO_TESTS=1
PASSTHRU=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-build) DO_BUILD=0 ;;
        --no-tests) DO_TESTS=0 ;;
        --help|-h)
            cat <<'USAGE'
Usage: scripts/dev.sh [--no-build] [--no-tests] [-- <devroom args>]

Builds the development executable, runs the deterministic test suite, and
launches the native Wayland/Vulkan devroom. Test failures stop the launch.
USAGE
            exit 0 ;;
        --)
            shift
            PASSTHRU+=("$@")
            break ;;
        *)
            PASSTHRU+=("$1") ;;
    esac
    shift
done

if [[ ${DO_BUILD} -eq 1 ]]; then
    "${ROOT}/scripts/build-dev.sh"
fi
if [[ ${DO_TESTS} -eq 1 ]]; then
    "${ROOT}/scripts/test.sh"
fi
exec "${ROOT}/scripts/run-devroom.sh" "${PASSTHRU[@]}"
