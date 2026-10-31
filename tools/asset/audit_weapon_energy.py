#!/usr/bin/env python3
"""Audit serialized weapon `consume` as the native energy cost."""
from __future__ import annotations
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCENE = ROOT / "analysis/assets/scenes/r1_1_visual.json"
OUT = ROOT / "analysis/assets/weapon_energy_audit.json"
MD = ROOT / "analysis/assets/weapon_energy_audit.md"

def main() -> int:
    scene = json.loads(SCENE.read_text(encoding="utf-8"))
    weapons = []
    for weapon in scene.get("weapons", []):
        weapons.append({
            "id": weapon.get("id"),
            "damage": weapon.get("damage"),
            "cycle_seconds": weapon.get("attack_cycle_seconds"),
            "serialized_consume": weapon.get("consume"),
            "native_energy_cost": weapon.get("energy_cost", weapon.get("consume")),
            "projectile": weapon.get("projectile", {}).get("id"),
            "classification": "CONFIRMED energy cost via serialized RGWeapon.consume; RGWeapon.MakeConsume/MultiHand.CheckEnergyEnough identify the energy path",
        })
    document = {
        "schema": 1,
        "tool": "tools/asset/audit_weapon_energy.py",
        "scene": "analysis/assets/scenes/r1_1_visual.json",
        "evidence": [
            "IL2CPP symbols: RGWeapon.MakeConsume, RGWeapon.GetDescConsume, MultiHand.CheckEnergyEnough, MultiHand.GetConsume, UICanvas.UpdateEnergyBar",
            "RoleAttributePlayer.EnergyReLoad disassembly confirms +1 energy per 2.0 s below max",
            "weapon_000 serialized consume=0; weapon_002/048/287 serialized consume=1",
        ],
        "weapons": weapons,
        "ammo": "No separate serialized ammo cost recovered for these weapons; do not claim an ammo system.",
    }
    OUT.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    lines = ["# Weapon Energy Cost Audit", "", "| Weapon | Damage | Cycle | Energy cost | Projectile |", "|---|---:|---:|---:|---|"]
    lines += [f"| {w['id']} | {w['damage']} | {w['cycle_seconds']} | {w['native_energy_cost']} | {w['projectile']} |" for w in weapons]
    lines += ["", "Serialized `consume` is the per-attack energy cost, confirmed by `RGWeapon.MakeConsume` and `MultiHand.CheckEnergyEnough`. No separate ammo cost is claimed.", ""]
    MD.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {OUT.relative_to(ROOT)} and {MD.relative_to(ROOT)}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
