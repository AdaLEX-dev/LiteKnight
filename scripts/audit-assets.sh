#!/usr/bin/env bash
# Audit committed metadata and, when available, local generated pixels.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
python3 "${ROOT}/tools/asset/audit_assets.py" --require-generated "$@"
python3 "${ROOT}/tools/asset/audit_ui_assets.py"
python3 "${ROOT}/tools/asset/audit_weapon_energy.py"
python3 "${ROOT}/tools/asset/audit_item_assets.py"
"${ROOT}/tools/asset/.venv/bin/python" "${ROOT}/tools/asset/audit_room_catalogue.py"
"${ROOT}/tools/asset/.venv/bin/python" "${ROOT}/tools/asset/audit_chest_catalogue.py"
"${ROOT}/tools/asset/.venv/bin/python" "${ROOT}/tools/asset/audit_map_catalogue.py"
"${ROOT}/tools/asset/.venv/bin/python" "${ROOT}/tools/asset/audit_enemy_catalogue.py"
python3 "${ROOT}/tools/asset/audit_weapon_runtime.py"
python3 "${ROOT}/tools/asset/audit_hub_topology.py"
python3 "${ROOT}/tools/asset/audit_game_catalogues.py"
if [[ -f "${ROOT}/analysis/assets/scenes/hero_room.json" ]]; then
    python3 "${ROOT}/tools/asset/audit_assets.py" \
        --scene "${ROOT}/analysis/assets/scenes/hero_room.json" \
        --inventory "${ROOT}/analysis/assets/asset_inventory.json" \
        --generated-root "${ROOT}/assets/generated/visual/hero_room" \
        --report-dir "${ROOT}/analysis/assets/hero-room" \
        --require-generated
fi
