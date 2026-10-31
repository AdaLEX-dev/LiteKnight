#!/usr/bin/env bash
# Audit committed metadata and, when available, local generated pixels.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
python3 "${ROOT}/tools/asset/audit_assets.py" --require-generated "$@"
if [[ -f "${ROOT}/analysis/assets/scenes/hero_room.json" ]]; then
    python3 "${ROOT}/tools/asset/audit_assets.py" \
        --scene "${ROOT}/analysis/assets/scenes/hero_room.json" \
        --inventory "${ROOT}/analysis/assets/asset_inventory.json" \
        --generated-root "${ROOT}/assets/generated/visual/hero_room" \
        --report-dir "${ROOT}/analysis/assets/hero-room" \
        --require-generated
fi
