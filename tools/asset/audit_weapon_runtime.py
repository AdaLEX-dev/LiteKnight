#!/usr/bin/env python3
"""Report how much of the recovered weapon catalogue is runtime-integrated."""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CATALOGUE = ROOT / "analysis/assets/weapon_catalogue.json"
SCENE = ROOT / "analysis/assets/scenes/r1_1_visual.json"
OUT_JSON = ROOT / "analysis/assets/weapon_runtime_integration.json"
OUT_MD = ROOT / "analysis/assets/weapon_runtime_integration.md"


def main() -> int:
    catalogue = json.loads(CATALOGUE.read_text(encoding="utf-8"))
    scene = json.loads(SCENE.read_text(encoding="utf-8"))
    integrated = {weapon["id"]: weapon for weapon in scene.get("weapons", [])}
    rows = []
    for weapon in catalogue["weapons"]:
        weapon_id = weapon["id"]
        if weapon_id in integrated:
            tier = "A"
            note = ("catalogue stats, visual, projectile, bullet count, spread "
                    "and attack cadence in native scene")
        elif weapon.get("is_melee"):
            tier = "B"
            note = ("serialized melee stats/name recovered; visual and melee "
                    "hitbox runtime pending")
        elif "damage" in weapon and "energy_cost" in weapon:
            tier = "C"
            note = ("stats recovered; unique, charge, throw or non-standard "
                    "mechanic not integrated")
        else:
            tier = "C"
            note = "catalogue-only name/metadata; no serializable runtime prefab"
        rows.append({
            "id": weapon_id,
            "display_name": weapon.get("display_name", weapon_id),
            "tier": tier,
            "has_stats": "damage" in weapon,
            "damage": weapon.get("damage"),
            "energy_cost": weapon.get("energy_cost"),
            "note": note,
        })
    counts = {
        "catalogue_entries": len(rows),
        "tier_a_fully_integrated": sum(1 for row in rows if row["tier"] == "A"),
        "tier_b_melee_pending": sum(
            1 for row in rows if row["tier"] == "B"),
        "tier_c_unique_or_catalogue_only": sum(
            1 for row in rows if row["tier"] == "C"),
    }
    document = {
        "schema": 1,
        "tool": "tools/asset/audit_weapon_runtime.py",
        "scene": "analysis/assets/scenes/r1_1_visual.json",
        "counts": counts,
        "note": "Tier A means the weapon is loaded by the native WeaponRegistry and can spawn, fire, be picked up and appear in the weapon station.",
        "weapons": rows,
    }
    OUT_JSON.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n",
                        encoding="utf-8")
    lines = [
        "# Weapon Runtime Integration",
        "",
        f"- catalogue entries: **{counts['catalogue_entries']}**",
        f"- Tier A (fully integrated): **{counts['tier_a_fully_integrated']}**",
        f"- Tier B (melee stats recovered, melee hitbox pending): **{counts['tier_b_melee_pending']}**",
        f"- Tier C (unique mechanic or catalogue-only): **{counts['tier_c_unique_or_catalogue_only']}**",
        "",
        "## Tier A",
        "",
        "| id | display name | damage | energy |",
        "|---|---|---:|---:|",
    ]
    for row in rows:
        if row["tier"] != "A":
            continue
        lines.append(f"| `{row['id']}` | {row['display_name']} | "
                     f"{row['damage']} | {row['energy_cost']} |")
    lines += [
        "",
        "## Tier B/C",
        "",
        "The full catalogue remains available for lookup and UI naming. Tier B",
        "weapons are not silently treated as complete: the native scene only",
        "bundles the recovered visual/projectile/cadence set for Tier A.",
        "",
    ]
    OUT_MD.write_text("\n".join(lines), encoding="utf-8")
    print(f"Tier A={counts['tier_a_fully_integrated']} "
          f"B={counts['tier_b_melee_pending']} "
          f"C={counts['tier_c_unique_or_catalogue_only']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
