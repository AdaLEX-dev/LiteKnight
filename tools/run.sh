#!/usr/bin/env bash
# Run the game
# Usage: ./tools/run.sh [Debug|RelWithDebInfo|Release] [args...]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"

CONFIG="${1:-Debug}"
shift || true
GAME_ARGS=("$@")

BINARY="${BUILD_DIR}/soul-knight"

echo "=== Soul Knight Linux Run ==="
echo "Config: ${CONFIG}"
echo "Binary: ${BINARY}"
if [ ${#GAME_ARGS[@]} -gt 0 ]; then
    echo "Args: ${GAME_ARGS[*]}"
fi
echo

# Verify binary exists
if [ ! -f "${BINARY}" ]; then
    echo "Binary not found: ${BINARY}"
    echo "Run ./tools/build.sh ${CONFIG} first"
    exit 1
fi

# Set up environment
export LD_LIBRARY_PATH="${BUILD_DIR}:${LD_LIBRARY_PATH:-}"

# XDG directories (for save/config/assets)
export XDG_DATA_HOME="${XDG_DATA_HOME:-${HOME}/.local/share}"
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-${HOME}/.config}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-${HOME}/.cache}"

# Create game directories
mkdir -p "${XDG_DATA_HOME}/soul-knight/saves"
mkdir -p "${XDG_DATA_HOME}/soul-knight/assets"
mkdir -p "${XDG_CONFIG_HOME}/soul-knight"
mkdir -p "${XDG_CACHE_HOME}/soul-knight/logs"

# Wayland-only target. Do not hardcode a Vulkan ICD; let the system loader
# select the installed driver.
export SDL_VIDEODRIVER=wayland

# Run
cd "${PROJECT_ROOT}"
echo "Starting Soul Knight..."
echo "---"
exec "${BINARY}" "${GAME_ARGS[@]}"