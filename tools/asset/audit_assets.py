#!/usr/bin/env python3
"""Validate the committed native scene metadata and asset inventory.

This audit never needs the proprietary reference bundle tree. It checks
metadata integrity, dangling references, duplicate identities, generated asset
presence, and redistribution bookkeeping.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SCENE = ROOT / "analysis/assets/scenes/r1_1_visual.json"
DEFAULT_INVENTORY = ROOT / "analysis/assets/asset_inventory.json"


class Audit:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []

    def error(self, message: str) -> None:
        self.errors.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)


def finite(*values: Any) -> bool:
    for value in values:
        if isinstance(value, (int, float)) and not math.isfinite(float(value)):
            return False
    return True


def audit(scene_path: Path, inventory_path: Path, require_generated: bool) -> Audit:
    result = Audit()
    scene = json.loads(scene_path.read_text(encoding="utf-8"))
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))

    texture_ids: dict[str, dict[str, Any]] = {}
    source_ids: dict[str, str] = {}
    for texture in scene.get("textures", []):
        texture_id = texture.get("id")
        if not texture_id:
            result.error("texture without id")
            continue
        if texture_id in texture_ids:
            result.error(f"duplicate texture id: {texture_id}")
        texture_ids[texture_id] = texture
        source_id = str(texture.get("source_path_id", ""))
        if source_id:
            if source_id in source_ids and source_ids[source_id] != texture_id:
                result.error(
                    f"conflicting source_path_id {source_id}: "
                    f"{source_ids[source_id]} vs {texture_id}")
            source_ids[source_id] = texture_id
        width = texture.get("width")
        height = texture.get("height")
        if not isinstance(width, int) or not isinstance(height, int) or width <= 0 or height <= 0:
            result.error(f"invalid texture dimensions: {texture_id}")
        # Scene metadata stores the serialized source format; the native
        # converter normalizes RGBA32/RGBA4444 to canonical RGBA8.
        if texture.get("format") not in ("RGBA32", "RGBA4444"):
            result.warn(f"unexpected source texture format: {texture_id} {texture.get('format')}")
        if require_generated:
            generated = ROOT / "assets/generated/visual/r1_1_visual" / texture.get("png", "")
            if not generated.is_file():
                result.error(f"missing generated texture: {generated.relative_to(ROOT)}")

    sprite_ids: set[str] = set()
    for sprite in scene.get("sprites", []):
        sprite_id = sprite.get("id")
        if not sprite_id:
            result.error("sprite without id")
            continue
        if sprite_id in sprite_ids:
            result.error(f"duplicate sprite id: {sprite_id}")
        sprite_ids.add(sprite_id)
        texture = texture_ids.get(sprite.get("texture"))
        if texture is None:
            result.error(f"sprite references missing texture: {sprite_id}")
            continue
        rect = sprite.get("rect", [])
        pivot = sprite.get("pivot", [])
        ppu = sprite.get("pixels_per_unit", 0)
        if len(rect) != 4 or not finite(*rect):
            result.error(f"invalid sprite rect: {sprite_id}")
            continue
        x, y, w, h = (float(value) for value in rect)
        if w <= 0 or h <= 0 or x < 0 or y < 0:
            result.error(f"non-positive sprite rect: {sprite_id}")
        if x + w > texture["width"] + 0.001 or y + h > texture["height"] + 0.001:
            result.error(f"sprite rect outside texture: {sprite_id}")
        if len(pivot) != 2 or not finite(*pivot) or not (0.0 <= pivot[0] <= 1.0 and 0.0 <= pivot[1] <= 1.0):
            result.error(f"invalid sprite pivot: {sprite_id}")
        if not isinstance(ppu, (int, float)) or not math.isfinite(float(ppu)) or ppu <= 0:
            result.error(f"invalid pixels_per_unit: {sprite_id}")

    animations = scene.get("animations", [])
    for animation in animations:
        path = ROOT / "analysis/assets/scenes" / animation
        if not path.is_file():
            result.error(f"missing animation metadata: {path.relative_to(ROOT)}")
    for character in scene.get("characters", []):
        for key in ("body_sprite", "dead_sprite"):
            if character.get(key) and character[key] not in sprite_ids:
                result.error(f"{character.get('id')} references missing {key}")
        for key in ("idle_animation", "run_animation"):
            if character.get(key) and f"animations/{character[key]}.json" not in animations:
                result.error(f"{character.get('id')} references missing {key}")
    for weapon in scene.get("weapons", []):
        if weapon.get("visual_sprite") not in sprite_ids:
            result.error(f"{weapon.get('id')} has missing visual sprite")
        projectile = weapon.get("projectile", {})
        if projectile.get("sprite") not in sprite_ids:
            result.error(f"{weapon.get('id')} has missing projectile sprite")
        if weapon.get("damage", 0) <= 0:
            result.error(f"{weapon.get('id')} has non-positive damage")
        if weapon.get("attack_cycle_seconds", 0) <= 0:
            result.error(f"{weapon.get('id')} has invalid attack cycle")
    for enemy in scene.get("enemy_definitions", []):
        for key in ("body_sprite", "dead_sprite", "shadow_sprite"):
            if enemy.get(key) not in sprite_ids:
                result.error(f"{enemy.get('id')} has missing {key}")
    for fixture in scene.get("enemy_fixtures", []):
        if fixture.get("definition") not in {e.get("id") for e in scene.get("enemy_definitions", [])}:
            result.error(f"enemy fixture references missing definition: {fixture.get('definition')}")

    summary = inventory.get("summary", {})
    if summary.get("textures_integrated") != len(scene.get("textures", [])):
        result.warn("inventory texture count differs from scene texture count")
    if summary.get("sprites_integrated") != len(scene.get("sprites", [])):
        result.warn("inventory sprite count differs from scene sprite count")
    local_only = sum(1 for texture in inventory.get("textures", [])
                     if texture.get("redistribution") == "local_only")
    unknown = sum(1 for texture in inventory.get("textures", [])
                  if texture.get("redistribution") == "unknown")
    if local_only:
        result.warn(f"{local_only} texture records are local_only (expected for reference-derived pixels)")
    if unknown:
        result.warn(f"{unknown} texture records have unknown redistribution status")
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scene", type=Path, default=DEFAULT_SCENE)
    parser.add_argument("--inventory", type=Path, default=DEFAULT_INVENTORY)
    parser.add_argument("--require-generated", action="store_true",
                        help="also fail when generated local texture PNGs are absent")
    args = parser.parse_args()
    result = audit(args.scene, args.inventory, args.require_generated)
    print(f"LiteKnight asset audit: {args.scene}")
    print(f"  errors: {len(result.errors)}  warnings: {len(result.warnings)}")
    for message in result.errors:
        print(f"  ERROR: {message}")
    for message in result.warnings:
        print(f"  WARN:  {message}")
    if result.errors:
        return 1
    print("  RESULT: metadata integrity PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
