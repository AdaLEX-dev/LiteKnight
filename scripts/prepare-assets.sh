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

copy_scene() {
    mkdir -p "$(dirname "${SCENE_DIR}")"
    if command -v rsync >/dev/null 2>&1; then
        rsync -a --delete "${SOURCE_DIR}/" "${SCENE_DIR}/"
    else
        rm -rf "${SCENE_DIR}"
        cp -a "${SOURCE_DIR}" "${SCENE_DIR}"
    fi
}
copy_scene
echo "Runtime scene: FOUND (${SCENE_DIR}/scene.json)"

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

# 5. Inventory + validation. These are metadata-only and safe in CI.
python3 "${ROOT}/tools/asset/build_asset_inventory.py"
python3 "${ROOT}/tools/asset/audit_assets.py" --require-generated
echo "=== Asset preparation complete ==="
