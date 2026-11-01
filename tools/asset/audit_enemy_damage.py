#!/usr/bin/env python3
"""Build the recovered enemy-damage reference used by the runtime test.

The runtime definitions are extracted from the APK into the recovered room
scenes. This audit freezes those exact values and cross-checks HP/controller
class against the independently extracted enemy catalogue. A future refactor
that silently replaces contact/projectile/explosion damage with a generic
number will fail the comparison test.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCENE = ROOT / "analysis/assets/scenes/r1_1_visual.json"
CATALOGUE = ROOT / "analysis/assets/enemies/enemy_catalogue.json"
OUTPUT = ROOT / "analysis/assets/scenes/enemy_damage_reference.json"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    scene = json.loads(SCENE.read_text(encoding="utf-8"))
    catalogue = {
        entry["id"]: entry
        for entry in json.loads(
            CATALOGUE.read_text(encoding="utf-8"))["entries"]
    }
    entries = []
    for enemy in scene.get("enemy_definitions", []):
        enemy_id = enemy.get("id", "")
        if not enemy_id:
            continue
        reference = catalogue.get(enemy_id)
        if reference is not None:
            role = reference.get("role", {})
            if int(role.get("max_hp", enemy.get("hp", 0))) != int(
                    enemy.get("hp", 0)):
                raise SystemExit(
                    f"HP mismatch for {enemy_id}: scene={enemy.get('hp')} "
                    f"catalogue={role.get('max_hp')}")
            controller_class = reference.get("controller", {}).get("class")
            if controller_class and controller_class != enemy.get("class_name"):
                raise SystemExit(
                    f"controller mismatch for {enemy_id}: "
                    f"scene={enemy.get('class_name')} catalogue={controller_class}")
        entries.append({
            "id": enemy_id,
            "class_name": enemy.get("class_name", ""),
            "hp": int(enemy.get("hp", 0)),
            "contact_damage": int(enemy.get("contact_damage", 0)),
            "projectile_damage": int(enemy.get("projectile_damage", 0)),
            "death_explosion_damage": int(
                enemy.get("death_explosion_damage", 0)),
            "contact_cooldown": float(enemy.get("contact_cooldown", 0.0)),
        })
    entries.sort(key=lambda item: item["id"])
    document = {
        "schema": 1,
        "tool": "tools/asset/audit_enemy_damage.py",
        "source_scene": str(SCENE.relative_to(ROOT)),
        "source_scene_sha256": sha256(SCENE),
        "source_catalogue_sha256": sha256(CATALOGUE),
        "classification": (
            "CONFIRMED serialized enemy weapon_atk/controller damage values "
            "from the recovered APK prefabs, cross-checked against RoleAttribute "
            "HP and RGEController class in enemy_catalogue.json."),
        "enemy_count": len(entries),
        "entries": entries,
    }
    OUTPUT.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    print(f"Wrote {OUTPUT.relative_to(ROOT)}: {len(entries)} enemies")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
