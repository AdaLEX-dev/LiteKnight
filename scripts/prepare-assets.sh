#!/usr/bin/env bash
# Copy generated native scene assets into a build tree.
# Set LITEKNIGHT_BUILD_DIR to select a build (default: <root>/build).
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LITEKNIGHT_BUILD_DIR:-${ROOT}/build}"
SCENE_DIR="${BUILD_DIR}/assets/scenes/r1_1_visual"
SOURCE_DIR="${ROOT}/assets/generated/visual/r1_1_visual"

copy_scene() {
    mkdir -p "$(dirname "${SCENE_DIR}")"
    if command -v rsync >/dev/null 2>&1; then
        rsync -a --delete "${SOURCE_DIR}/" "${SCENE_DIR}/"
    else
        rm -rf "${SCENE_DIR}"
        cp -a "${SOURCE_DIR}" "${SCENE_DIR}"
    fi
}

if [[ ! -f "${SCENE_DIR}/scene.json" ]]; then
    if [[ -f "${SOURCE_DIR}/scene.json" ]]; then
        copy_scene
        echo "Copied generated devroom assets to ${SCENE_DIR}"
    elif [[ -x "${ROOT}/tools/asset/.venv/bin/python" && -d "${ROOT}/analysis/apktool_out/assets/AssetBundles" ]]; then
        echo "Generated devroom assets missing; rebuilding from local reference..."
        "${ROOT}/tools/asset/.venv/bin/python" "${ROOT}/tools/asset/build_visual_scene.py"
        copy_scene
    else
        echo "ERROR: no generated devroom assets and no local reference bundle tree." >&2
        echo "The proprietary reference assets are intentionally not committed." >&2
        echo "Provide the local reference tree and run tools/asset/build_visual_scene.py," >&2
        echo "or place a prepared scene in ${SCENE_DIR}." >&2
        exit 2
    fi
elif [[ -f "${SOURCE_DIR}/scene.json" ]]; then
    # Keep a rebuilt local scene in sync without deleting unrelated build files.
    copy_scene
fi
