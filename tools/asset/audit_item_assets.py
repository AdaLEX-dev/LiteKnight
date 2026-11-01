#!/usr/bin/env python3
"""Validate the recovered chest/coin item pack metadata."""
from __future__ import annotations
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
META = ROOT / "analysis/assets/items/item_assets.json"
OUT = ROOT / "analysis/assets/items/item_audit.json"
MD = ROOT / "analysis/assets/items/item_audit.md"

def main() -> int:
    data = json.loads(META.read_text(encoding="utf-8"))
    chests = data.get("chests", [])
    coins = data.get("coins", [])
    errors = []
    if not chests:
        errors.append("no chest definitions")
    if not coins:
        errors.append("no coin definitions")
    for coin in coins:
        if not coin.get("frames"):
            errors.append(f"{coin.get('id')} has no frames")
        if int(coin.get("value", 0)) <= 0:
            errors.append(f"{coin.get('id')} has no positive value")
    for chest in chests:
        if not chest.get("visuals"):
            errors.append(f"{chest.get('id')} has no visuals")
        has_coin = bool(chest.get("loot_coin"))
        has_weapon = bool(chest.get("appointed_weapon")) or \
            int(chest.get("weapon_drop_level", -1)) >= 0
        if not has_coin and not has_weapon:
            errors.append(f"{chest.get('id')} has no coin or weapon loot")
    document = {
        "schema": 1,
        "tool": "tools/asset/audit_item_assets.py",
        "source": META.relative_to(ROOT).as_posix(),
        "chests": len(chests),
        "coins": len(coins),
        "coin_values": {coin["id"]: coin["value"] for coin in coins},
        "coin_frame_counts": {coin["id"]: len(coin.get("frames", []))
                              for coin in coins},
        "evidence": [
            "chest00/01/02 prefabs: Animator + RGChest table and chest_open 1.0 s",
            "coin_0/1/2 prefab RGCoin: type 0/1/2 and value 5/3/1, speed 20, range 7",
            "WeaponDropInfo: 7 weighted levels recovered",
            "RGChestAppoint fixed object_list and RGChest weapon drop path",
        ],
        "native_extension": "integrated chests use RGChestAppoint weapon_021 and WeaponDropInfo levels 1/2; coin loot remains a separate recovered coin path",
        "errors": errors,
    }
    OUT.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n",
                   encoding="utf-8")
    lines = ["# Chest / Coin Asset Audit", "",
             "| Item | Evidence | Value / frames |", "|---|---|---|"]
    for coin in coins:
        lines.append(f"| {coin['id']} | RGCoin type {coin['type']} | "
                     f"value {coin['value']}, {len(coin.get('frames', []))} frames |")
    for chest in chests:
        weapon_loot = chest.get("appointed_weapon", "")
        if not weapon_loot and int(chest.get("weapon_drop_level", -1)) >= 0:
            weapon_loot = f"WeaponDropInfo level {chest.get('weapon_drop_level')}"
        lines.append(f"| {chest['id']} | RGChest level {chest.get('level', 0)} | "
                     f"weapon {weapon_loot or '-'}; coin {chest.get('loot_coin') or '-'} |")
    lines += ["", "Chest_open duration 1.0 s comes from the recovered Animator clip.",
              "RGChest weapon selection uses recovered WeaponDropInfo; exact boss/big/",
              "material distributions remain UNKNOWN.", ""]
    MD.write_text("\n".join(lines), encoding="utf-8")
    if errors:
        raise SystemExit("; ".join(errors))
    print(f"Wrote {OUT.relative_to(ROOT)} and {MD.relative_to(ROOT)}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
