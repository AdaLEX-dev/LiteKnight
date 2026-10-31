#!/usr/bin/env python3
"""Build the source-controlled LiteKnight asset inventory.

The inventory contains metadata/provenance only. Proprietary pixels remain in
the gitignored generated asset tree and are never copied into this database.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SCENE = ROOT / "analysis/assets/scenes/r1_1_visual.json"
DEFAULT_TEXTURE_OUT = ROOT / "analysis/assets/texture_inventory.json"
DEFAULT_ASSET_OUT = ROOT / "analysis/assets/asset_inventory.json"

# Observed reference catalogue counts from local UnityPy scans. These are
# metadata observations, not claims that every asset is integrated.
REFERENCE_OBSERVED = {
    "textures": 365,
    "sprites": 6408,
    "characters": 15,
    "weapon_ids": 365,
    "enemy_ids": 125,
}

REDISTRIBUTION_LOCAL = "local_only"
REDISTRIBUTION_ALLOWED = "allowed"
REDISTRIBUTION_UNKNOWN = "unknown"


def sha256_file(path: Path) -> str | None:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def lifecycle(discovered: int, extracted: int, translated: int,
              validated: int, integrated: int, tested: int,
              note: str = "") -> dict[str, Any]:
    """Explicit asset maturity ladder used by the asset laboratory."""
    return {
        "DISCOVERED": discovered,
        "EXTRACTED": extracted,
        "TRANSLATED": translated,
        "VALIDATED": validated,
        "INTEGRATED": integrated,
        "DEVROOM_TESTED": tested,
        "highest": (
            "DEVROOM_TESTED" if tested > 0 else
            "INTEGRATED" if integrated > 0 else
            "VALIDATED" if validated > 0 else
            "TRANSLATED" if translated > 0 else
            "EXTRACTED" if extracted > 0 else
            "DISCOVERED"
        ),
        "note": note,
    }


def build(scene_path: Path) -> tuple[dict[str, Any], dict[str, Any]]:
    scene = json.loads(scene_path.read_text(encoding="utf-8"))
    sprites_by_texture: dict[str, list[dict[str, Any]]] = {}
    for sprite in scene.get("sprites", []):
        sprites_by_texture.setdefault(sprite["texture"], []).append(sprite)

    texture_records = []
    for texture in scene.get("textures", []):
        texture_id = texture["id"]
        sprites = sprites_by_texture.get(texture_id, [])
        generated = ROOT / "assets/generated/visual/r1_1_visual" / texture.get("png", "")
        texture_records.append({
            "id": texture_id,
            "source_bundle": texture.get("source_bundle"),
            "source_path_id": texture.get("source_path_id"),
            "source_name": texture.get("source_name"),
            "source_asset": f"{texture.get('source_bundle')}:{texture.get('source_path_id')}",
            "dimensions": [texture.get("width"), texture.get("height")],
            "format": texture.get("format"),
            "filter_mode": texture.get("filter"),
            "wrap_mode": texture.get("wrap"),
            "rgba8_sha256": texture.get("rgba8_sha256"),
            "png_sha256": texture.get("png_sha256"),
            "native_output": texture.get("png"),
            "generated_png": str(generated.relative_to(ROOT)),
            "generated_png_exists": generated.is_file(),
            "sprite_count": len(sprites),
            "sprite_ids": [sprite["id"] for sprite in sprites],
            "pixels_per_unit_values": sorted({
                float(sprite.get("pixels_per_unit", 0.0)) for sprite in sprites
            }),
            "atlas_information": {
                "is_atlas": len(sprites) > 1,
                "size": [texture.get("width"), texture.get("height")],
                "sprite_count": len(sprites),
            },
            "status": "INTEGRATED" if sprites else "EXTRACTED",
            "lifecycle_status": "INTEGRATED" if sprites else "EXTRACTED",
            "redistribution": REDISTRIBUTION_LOCAL,
            "license_status": "UNKNOWN",
            "note": "Reference-derived pixels are local development assets; do not redistribute.",
        })

    characters = []
    for character in scene.get("characters", []):
        characters.append({
            "id": character.get("id"),
            "source_path_id": character.get("source_path_id"),
            "controller_name": character.get("controller_name"),
            "body_sprite": character.get("body_sprite"),
            "dead_sprite": character.get("dead_sprite"),
            "idle_animation": character.get("idle_animation"),
            "run_animation": character.get("run_animation"),
            "status": "INTEGRATED_IDLE_RUN",
            "lifecycle_status": "DEVROOM_TESTED" if character.get("id") == "c00" else "INTEGRATED",
            "missing": ["attack/charge animation", "hit animation", "full dead animation curves", "display name"],
            "redistribution": REDISTRIBUTION_LOCAL,
            "license_status": "UNKNOWN",
        })

    weapons = []
    for weapon in scene.get("weapons", []):
        weapons.append({
            "id": weapon.get("id"),
            "visual_sprite": weapon.get("visual_sprite"),
            "projectile": weapon.get("projectile", {}).get("id"),
            "projectile_sprite": weapon.get("projectile", {}).get("sprite"),
            "damage": weapon.get("damage"),
            "bullet_speed": weapon.get("bullet_speed"),
            "attack_cycle_seconds": weapon.get("attack_cycle_seconds"),
            "consume": weapon.get("consume"),
            "status": "INTEGRATED_INSPECTION_FIRE",
            "lifecycle_status": "DEVROOM_TESTED",
            "missing": ["item display name", "pickup/drop identity", "weapon-specific special effects"],
            "redistribution": REDISTRIBUTION_LOCAL,
            "license_status": "UNKNOWN",
        })

    enemies = []
    for enemy in scene.get("enemy_definitions", []):
        enemies.append({
            "id": enemy.get("id"),
            "body_sprite": enemy.get("body_sprite"),
            "dead_sprite": enemy.get("dead_sprite"),
            "shadow_sprite": enemy.get("shadow_sprite"),
            "hp": enemy.get("hp"),
            "contact_damage": enemy.get("contact_damage"),
            "contact_cooldown": enemy.get("contact_cooldown"),
            "status": "INTEGRATED_SPAWN_DAMAGE_DEATH",
            "lifecycle_status": "DEVROOM_TESTED",
            "missing": ["full EnemyAI04 scouting/pathfinding", "death effect object", "room-clear lifecycle"],
            "redistribution": REDISTRIBUTION_LOCAL,
            "license_status": "UNKNOWN",
        })

    unique_projectiles = sorted({
        weapon.get("projectile", {}).get("id", "")
        for weapon in scene.get("weapons", [])
        if weapon.get("projectile", {}).get("id")
    })
    animations = []
    for animation in scene.get("animations", []):
        path = ROOT / "analysis/assets/scenes" / animation
        animations.append({
            "id": Path(animation).stem,
            "metadata": animation,
            "status": "INTEGRATED" if path.is_file() else "MISSING_METADATA",
            "redistribution": REDISTRIBUTION_LOCAL,
            "license_status": "UNKNOWN",
        })

    categories = [
        {"category": "characters", "integrated": len(characters),
         "observed_reference_candidates": REFERENCE_OBSERVED["characters"],
         "status": "DEVROOM_TESTED_PARTIAL",
         "lifecycle": lifecycle(15, 15, 15, 15, len(characters), 1,
                                "c00 live; c01..c14 integrated for selector/idle/run"),
         "note": "15 recovered c00..c14; idle/run integrated"},
        {"category": "weapons", "integrated": len(weapons),
         "observed_reference_candidates": REFERENCE_OBSERVED["weapon_ids"],
         "status": "DEVROOM_TESTED_PARTIAL",
         "lifecycle": lifecycle(365, 4, 4, 4, len(weapons), 4,
                                "four recovered Gun001 weapons: fire/equip/pickup"),
         "note": "first four Gun001 weapons integrated"},
        {"category": "projectiles", "integrated": len(unique_projectiles),
         "observed_reference_candidates": None, "status": "DEVROOM_TESTED_PARTIAL",
         "lifecycle": lifecycle(3, 3, 3, 3, len(unique_projectiles), 3,
                                "bullet_0/bullet_11/bullet_14 integrated"),
         "note": "projectile prefabs used by the four integrated weapons"},
        {"category": "enemies", "integrated": len(enemies),
         "observed_reference_candidates": REFERENCE_OBSERVED["enemy_ids"],
         "status": "DEVROOM_TESTED",
         "lifecycle": lifecycle(125, 1, 1, 1, len(enemies), 1,
                                "e_slime01 pursuit/contact/damage/death/live reset"),
         "note": "e_slime01 developer fixture only"},
        {"category": "walls", "integrated": (
            scene.get("topology", {}).get("wall_cell_count", 0) +
            scene.get("topology", {}).get("corridor_wall_cell_count", 0)),
         "status": "DEVROOM_TESTED",
         "lifecycle": lifecycle(420, 420, 420, 420, 420, 420,
                                "recovered room/corridor wall cells"),
         "note": "420 wall sprites across 140 room/corridor cells"},
        {"category": "floors", "integrated": 1, "status": "DEVROOM_TESTED",
         "lifecycle": lifecycle(1, 1, 1, 1, 1, 1,
                                "recovered floor atlas"),
         "note": "recovered floor atlas; full floor decoration inventory pending"},
        {"category": "doors", "integrated": len(scene.get("collision", {}).get("doors", [])),
         "status": "DEVROOM_TESTED",
         "lifecycle": lifecycle(4, 4, 4, 4,
                                len(scene.get("collision", {}).get("doors", [])), 4,
                                "open/close animation, collision timing and triggers"),
         "note": "four animated doors/triggers"},
        {"category": "obstacles", "integrated": len(scene.get("obstacles", {}).get("variants", [])),
         "status": "DEVROOM_TESTED",
         "lifecycle": lifecycle(12, 12, 12, 12,
                                len(scene.get("obstacles", {}).get("variants", [])), 12,
                                "selector/cycling and collider variants"),
         "note": "12 recovered obstacle_list variants; 8 solid selectable"},
        {"category": "effects", "integrated": 0, "status": "UNRECOVERED",
         "lifecycle": lifecycle(305, 0, 0, 0, 0, 0,
                                "ParticleSystem objects discovered, no native effects"),
         "note": "muzzle/hit/death effects not integrated"},
        {"category": "ui", "integrated": 0, "status": "PLACEHOLDER",
         "lifecycle": lifecycle(139, 0, 0, 0, 0, 0,
                                "reference UI names discovered; developer text atlas is PLACEHOLDER UI"),
         "note": "generated developer text atlas; recovered UI atlas not integrated"},
        {"category": "items/pickups", "integrated": 0, "status": "UNRECOVERED",
         "lifecycle": lifecycle(68, 0, 0, 0, 0, 0,
                                "name-heuristic discovery only"),
         "note": "weapon pickup foundation exists; item/pickup catalogue not translated"},
        {"category": "environment decorations", "integrated": 0, "status": "UNRECOVERED",
         "lifecycle": lifecycle(80, 0, 0, 0, 0, 0,
                                "name-heuristic discovery only")},
        {"category": "audio", "integrated": 0, "status": "UNRECOVERED",
         "lifecycle": lifecycle(288, 0, 0, 0, 0, 0,
                                "AudioClip discovery only")},
    ]

    texture_inventory = {
        "schema": 1,
        "tool": "tools/asset/build_asset_inventory.py",
        "scene": str(scene_path.relative_to(ROOT)),
        "scene_sha256": sha256_file(scene_path),
        "note": "Metadata/provenance only. No proprietary pixels are stored here.",
        "summary": {
            "textures_integrated": len(texture_records),
            "sprites_integrated": len(scene.get("sprites", [])),
            "textures_observed_in_reference": REFERENCE_OBSERVED["textures"],
            "sprites_observed_in_reference": REFERENCE_OBSERVED["sprites"],
            "redistribution_local_only": len(texture_records),
            "redistribution_allowed": 0,
            "redistribution_unknown": 0,
        },
        "textures": texture_records,
    }

    asset_inventory = {
        "schema": 1,
        "tool": "tools/asset/build_asset_inventory.py",
        "scene": str(scene_path.relative_to(ROOT)),
        "scene_sha256": sha256_file(scene_path),
        "versioning": {
            "source_controlled": ["scene metadata", "inventory manifests", "animations metadata"],
            "generated_deterministically": ["assets/generated/visual/r1_1_visual/"],
            "local_only": ["reference APK/bundle trees", "generated PNG/RGBA pixels"],
            "reference_only": ["original APK", "apktool/jadx/il2cpp outputs"],
        },
        "redistribution_policy": {
            "allowed": "project-authored source/configuration and generated developer font placeholder",
            "restricted": "none currently",
            "unknown": "unreviewed recovered content",
            "local_only": "reference-derived proprietary pixels and extracted data",
        },
        "summary": {
            "textures_integrated": len(texture_records),
            "sprites_integrated": len(scene.get("sprites", [])),
            "characters_integrated": len(characters),
            "weapons_integrated": len(weapons),
            "unique_projectiles_integrated": len(unique_projectiles),
            "enemies_integrated": len(enemies),
            "animations_integrated": len(animations),
            "doors_integrated": len(scene.get("collision", {}).get("doors", [])),
            "obstacle_variants": len(scene.get("obstacles", {}).get("variants", [])),
            "reference_candidates_observed": REFERENCE_OBSERVED,
            "reference_catalogue": "analysis/assets/reference_catalogue.json",
            "placeholders": ["developer 5x7 text atlas (PLACEHOLDER)"],
            "not_integrated_categories": sum(
                1 for item in categories if item["status"] in ("UNRECOVERED", "PLACEHOLDER")
            ),
            "lifecycle_statuses": [
                "DISCOVERED", "EXTRACTED", "TRANSLATED", "VALIDATED",
                "INTEGRATED", "DEVROOM_TESTED"
            ],
        },
        "categories": categories,
        "characters": characters,
        "weapons": weapons,
        "projectiles": unique_projectiles,
        "enemies": enemies,
        "animations": animations,
        "textures": texture_records,
    }
    return texture_inventory, asset_inventory


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scene", type=Path, default=DEFAULT_SCENE)
    parser.add_argument("--texture-out", type=Path, default=DEFAULT_TEXTURE_OUT)
    parser.add_argument("--asset-out", type=Path, default=DEFAULT_ASSET_OUT)
    args = parser.parse_args()
    texture_inventory, asset_inventory = build(args.scene)
    args.texture_out.parent.mkdir(parents=True, exist_ok=True)
    args.texture_out.write_text(json.dumps(texture_inventory, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    args.asset_out.write_text(json.dumps(asset_inventory, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Wrote {args.texture_out.relative_to(ROOT)}")
    print(f"Wrote {args.asset_out.relative_to(ROOT)}")
    print(json.dumps(asset_inventory["summary"], indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
