#!/usr/bin/env bash
# Prepare the complete local native asset environment.
#
# Repository rule: proprietary reference pixels are never committed.
# Local rule: this script may use the full local APK/bundle tree and generated
# native assets freely for development.
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LITEKNIGHT_BUILD_DIR:-${ROOT}/build}"
SCENE_DIR="${BUILD_DIR}/assets/scenes/r1_1_visual"
SOURCE_DIR="${ROOT}/assets/generated/visual/r1_1_visual"
HUB_SCENE_DIR="${BUILD_DIR}/assets/scenes/hero_room"
HUB_SOURCE_DIR="${ROOT}/assets/generated/visual/hero_room"
APK="${LITEKNIGHT_REFERENCE_APK:-${ROOT}/original/SoulKnight-2.6.8.apk}"
BUNDLE_ROOT="${ROOT}/analysis/apktool_out/assets/AssetBundles"
EXPECTED_APK_SHA256="8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea"
CATALOGUE="${ROOT}/analysis/assets/reference_catalogue.json"
FORCE_BUILD=0
REFRESH_CATALOGUE=0
EXTRACT=0

for arg in "$@"; do
    case "$arg" in
        --force-build) FORCE_BUILD=1 ;;
        --refresh-catalogue) REFRESH_CATALOGUE=1 ;;
        --extract) EXTRACT=1 ;;
        --help|-h)
            cat <<'USAGE'
Usage: scripts/prepare-assets.sh [--force-build] [--refresh-catalogue] [--extract]

Locates/verifies the local reference APK, extracts/generates native assets when
needed, refreshes the inventory/catalogue, validates metadata and generated
pixels, and copies the runtime scene into the build tree. --extract runs apktool
when the reference extraction tree is missing.

Environment:
  LITEKNIGHT_BUILD_DIR       build tree (default: <root>/build)
  LITEKNIGHT_REFERENCE_APK   local APK path
USAGE
            exit 0 ;;
        *) echo "Unknown argument: $arg" >&2; exit 2 ;;
    esac
done

echo "=== LiteKnight Asset Preparation ==="

# 1. Reference APK discovery/verification. A prepared generated scene is allowed
#    without the APK, but an APK that exists must match the expected version.
if [[ -f "${APK}" ]]; then
    actual_sha="$(sha256sum "${APK}" | awk '{print $1}')"
    if [[ "${actual_sha}" != "${EXPECTED_APK_SHA256}" ]]; then
        echo "ERROR: reference APK SHA-256 mismatch" >&2
        echo "  expected: ${EXPECTED_APK_SHA256}" >&2
        echo "  actual:   ${actual_sha}" >&2
        exit 2
    fi
    echo "Reference APK: FOUND (${APK}) SHA-256 verified"
else
    echo "Reference APK: MISSING (${APK})"
fi

# 2. Extraction/bundle tree. Do not silently destroy or overwrite it. If it is
#    missing but the APK is present, require the developer to run apktool rather
#    than attempting a surprising multi-hundred-MB operation.
if [[ -d "${BUNDLE_ROOT}" ]]; then
    echo "Reference extraction: FOUND (${BUNDLE_ROOT})"
elif [[ ${EXTRACT} -eq 1 ]]; then
    if [[ ! -f "${APK}" ]]; then
        echo "ERROR: --extract requires the reference APK at ${APK}" >&2
        exit 2
    fi
    if ! command -v apktool >/dev/null 2>&1; then
        echo "ERROR: --extract requires apktool on PATH" >&2
        exit 2
    fi
    echo "Extracting reference APK with apktool..."
    apktool d -f -o "${ROOT}/analysis/apktool_out" "${APK}"
    echo "Reference extraction: FOUND (${BUNDLE_ROOT})"
else
    echo "Reference extraction: MISSING (${BUNDLE_ROOT})"
    echo "  Re-run with --extract to run apktool, or extract manually:"
    echo "    apktool d -f -o ${ROOT}/analysis/apktool_out ${APK}"
fi

# 3. Generate the native scene only when absent or explicitly forced. The
#    builder is deterministic and writes to the gitignored generated tree.
if [[ ${FORCE_BUILD} -eq 1 || ! -f "${SOURCE_DIR}/scene.json" ]]; then
    if [[ ! -x "${ROOT}/tools/asset/.venv/bin/python" || ! -d "${BUNDLE_ROOT}" ]]; then
        echo "ERROR: cannot generate native assets without UnityPy venv + reference bundles." >&2
        exit 2
    fi
    echo "Generating native scene metadata/textures..."
    "${ROOT}/tools/asset/.venv/bin/python" "${ROOT}/tools/asset/build_visual_scene.py"
else
    echo "Native generated scene: FOUND (${SOURCE_DIR}/scene.json)"
fi

# Additional actual PatternRooms r1_2/r1_3. Generate from the local room bundle
# when absent; each has its own metadata, generated pixels and wall map.
for room in r1_2 r1_3; do
    room_source="${ROOT}/assets/generated/visual/${room}_visual"
    if [[ ${FORCE_BUILD} -eq 1 || ! -f "${room_source}/scene.json" ]]; then
        if [[ ! -x "${ROOT}/tools/asset/.venv/bin/python" || ! -d "${BUNDLE_ROOT}/room" ]]; then
            echo "ERROR: cannot generate ${room} without UnityPy venv + room bundle." >&2
            exit 2
        fi
        echo "Generating ${room} native scene metadata/textures..."
        "${ROOT}/tools/asset/.venv/bin/python" "${ROOT}/tools/asset/build_visual_scene.py" \
            --room "${room}" \
            --out "${room_source}" \
            --scene "${ROOT}/analysis/assets/scenes/${room}_visual.json" \
            --manifest "${ROOT}/analysis/assets/scenes/${room}_visual_manifest.json" \
            --wall-map "${ROOT}/analysis/re/${room}_wall_map.json" \
            --spawn 20 15
    fi
done

copy_scene() {
    local source_dir="$1"
    local scene_dir="$2"
    mkdir -p "$(dirname "${scene_dir}")"
    if command -v rsync >/dev/null 2>&1; then
        rsync -a --delete "${source_dir}/" "${scene_dir}/"
    else
        rm -rf "${scene_dir}"
        cp -a "${source_dir}" "${scene_dir}"
    fi
}
copy_scene "${SOURCE_DIR}" "${SCENE_DIR}"
echo "Runtime scene: FOUND (${SCENE_DIR}/scene.json)"
for room in r1_2_visual r1_3_visual; do
    source_dir="${ROOT}/assets/generated/visual/${room}"
    scene_dir="${BUILD_DIR}/assets/scenes/${room}"
    if [[ ! -f "${source_dir}/scene.json" ]]; then
        echo "ERROR: generated dungeon room missing: ${source_dir}/scene.json" >&2
        exit 2
    fi
    copy_scene "${source_dir}" "${scene_dir}"
    echo "Runtime scene: FOUND (${scene_dir}/scene.json)"
done

# 3a. Actual chest00/RGCoin item pack (prefab sprites + coin values).
ITEM_SOURCE="${ROOT}/assets/generated/items"
ITEM_TARGET="${BUILD_DIR}/assets/scenes/items"
if [[ ${FORCE_BUILD} -eq 1 || ! -f "${ROOT}/analysis/assets/items/item_assets.json" || ! -d "${ITEM_SOURCE}/textures" ]] || \
   ! grep -q '"portals"' "${ROOT}/analysis/assets/items/item_assets.json" || \
   ! grep -q '"weapon_drops"' "${ROOT}/analysis/assets/items/item_assets.json"; then
    if [[ ! -x "${ROOT}/tools/asset/.venv/bin/python" || ! -f "${BUNDLE_ROOT}/common" ]]; then
        echo "ERROR: cannot generate chest/coin item pack without UnityPy venv + common bundle." >&2
        exit 2
    fi
    echo "Generating chest/coin item pack..."
    "${ROOT}/tools/asset/.venv/bin/python" "${ROOT}/tools/asset/build_item_assets.py"
fi
if [[ -f "${ROOT}/analysis/assets/items/item_assets.json" ]]; then
    mkdir -p "${ITEM_TARGET}"
    if command -v rsync >/dev/null 2>&1; then
        rsync -a --delete "${ROOT}/analysis/assets/items/" "${ITEM_TARGET}/"
        mkdir -p "${ITEM_TARGET}/textures"
        rsync -a "${ITEM_SOURCE}/textures/" "${ITEM_TARGET}/textures/"
    else
        cp -a "${ROOT}/analysis/assets/items/." "${ITEM_TARGET}/"
        mkdir -p "${ITEM_TARGET}/textures"
        cp -a "${ITEM_SOURCE}/textures/." "${ITEM_TARGET}/textures/"
    fi
    cp -f "${ROOT}/analysis/assets/items/item_assets.json" "${ITEM_TARGET}/items.json"
    if [[ -f "${ROOT}/analysis/assets/weapon_catalogue.json" ]]; then
        cp -f "${ROOT}/analysis/assets/weapon_catalogue.json" "${ITEM_TARGET}/weapon_catalogue.json"
    fi
    echo "Runtime item pack: FOUND (${ITEM_TARGET}/items.json)"
fi

# 3b. Reference audio clips. Generated WAVs are local-only; the manifest is
# committed under analysis/.
AUDIO_SOURCE="${ROOT}/assets/generated/audio"
AUDIO_TARGET="${BUILD_DIR}/assets/audio"
if [[ ${FORCE_BUILD} -eq 1 || ! -d "${AUDIO_SOURCE}" || ! -f "${AUDIO_SOURCE}/pickup.wav" ]]; then
    if [[ -x "${ROOT}/tools/asset/.venv/bin/python" && -d "${BUNDLE_ROOT}" ]]; then
        echo "Building reference audio set..."
        "${ROOT}/tools/asset/.venv/bin/python" "${ROOT}/tools/asset/build_audio_assets.py"
    fi
fi
if [[ -d "${AUDIO_SOURCE}" ]]; then
    mkdir -p "${AUDIO_TARGET}"
    cp -a "${AUDIO_SOURCE}/." "${AUDIO_TARGET}/"
    cp -f "${ROOT}/analysis/assets/audio/audio_catalogue.json" "${AUDIO_TARGET}/audio_catalogue.json"
    echo "Runtime audio: FOUND (${AUDIO_TARGET})"
fi

# 3b. Living Room / HeroRoom. The builder joins the local split sharedassets
#     chunks and extracts level2 tilemaps, sprites and colliders.
if [[ ${FORCE_BUILD} -eq 1 || ! -f "${HUB_SOURCE_DIR}/scene.json" ]]; then
    if [[ -x "${ROOT}/tools/asset/.venv/bin/python" && -f "${ROOT}/analysis/apktool_out/assets/bin/Data/level2" ]]; then
        echo "Building Living Room scene metadata/textures..."
        "${ROOT}/tools/asset/.venv/bin/python" "${ROOT}/tools/asset/build_hub_scene.py"
    else
        echo "Living Room assets: SKIPPED (local reference level2 unavailable)"
    fi
fi
if [[ -f "${HUB_SOURCE_DIR}/scene.json" ]]; then
    mkdir -p "${HUB_SCENE_DIR}"
    if command -v rsync >/dev/null 2>&1; then
        rsync -a --delete "${HUB_SOURCE_DIR}/" "${HUB_SCENE_DIR}/"
    else
        rm -rf "${HUB_SCENE_DIR}"
        cp -a "${HUB_SOURCE_DIR}" "${HUB_SCENE_DIR}"
    fi
    echo "Runtime Living Room scene: FOUND (${HUB_SCENE_DIR}/scene.json)"
fi

# 4. Reference catalogue/discovery. The full scan is slower, so run it only when
#    missing or explicitly requested.
if [[ ${REFRESH_CATALOGUE} -eq 1 || ! -f "${CATALOGUE}" ]]; then
    if [[ -d "${BUNDLE_ROOT}" && -x "${ROOT}/tools/asset/.venv/bin/python" ]]; then
        echo "Discovering reference catalogue..."
        "${ROOT}/tools/asset/.venv/bin/python" "${ROOT}/tools/asset/discover_reference.py"
    else
        echo "Reference catalogue: SKIPPED (no reference bundles)"
    fi
else
    echo "Reference catalogue: FOUND (${CATALOGUE})"
fi

# 4a. Recovered UI atlases/title logo (local-only pixels).
if [[ ! -f "${ROOT}/assets/generated/ui/manifest.json" ]] || \
   ! grep -q '"schema": 2' "${ROOT}/assets/generated/ui/manifest.json" || \
   ! grep -q '"name": "menu_backdrop"' "${ROOT}/assets/generated/ui/manifest.json"; then
    if [[ -d "${BUNDLE_ROOT}" && -x "${ROOT}/tools/asset/.venv/bin/python" ]]; then
        echo "Building local UI atlas manifest..."
        "${ROOT}/tools/asset/.venv/bin/python" "${ROOT}/tools/asset/build_ui_scene.py"
    fi
fi

# 4b. Local pixel font atlas (reference-derived TTF rasterized locally). The
#     runtime falls back to the developer atlas when this is absent.
if [[ ! -f "${ROOT}/assets/generated/ui/font/pixel_bold.json" ]]; then
    if [[ -d "${BUNDLE_ROOT}" && -x "${ROOT}/tools/asset/.venv/bin/python" ]]; then
        echo "Building local pixel font atlas..."
        "${ROOT}/tools/asset/.venv/bin/python" "${ROOT}/tools/asset/build_ui_font.py"
    else
        echo "Pixel font atlas: SKIPPED (local reference font unavailable)"
    fi
fi
if [[ -d "${ROOT}/assets/generated/ui" ]]; then
    mkdir -p "${BUILD_DIR}/assets/ui"
    if command -v rsync >/dev/null 2>&1; then
        rsync -a --delete "${ROOT}/assets/generated/ui/" "${BUILD_DIR}/assets/ui/"
    else
        cp -a "${ROOT}/assets/generated/ui/." "${BUILD_DIR}/assets/ui/"
    fi
    echo "Runtime UI assets: FOUND (${BUILD_DIR}/assets/ui)"
fi

# 5. Inventory + validation. These are metadata-only and safe in CI.
python3 "${ROOT}/tools/asset/build_asset_inventory.py"
python3 "${ROOT}/tools/asset/audit_assets.py" --require-generated
echo "=== Asset preparation complete ==="
