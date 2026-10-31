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
        self.counts: dict[str, int] = {}

    def error(self, message: str) -> None:
        self.errors.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)


def finite(*values: Any) -> bool:
    for value in values:
        if isinstance(value, (int, float)) and not math.isfinite(float(value)):
            return False
    return True


def audit(scene_path: Path, inventory_path: Path, require_generated: bool,
          generated_root: Path | None = None) -> Audit:
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
        source_cab = str(texture.get("source_cab", ""))
        source_name = str(texture.get("source_name", ""))
        source_key = (source_cab, source_id, source_name)
        if source_id:
            if source_key in source_ids and source_ids[source_key] != texture_id:
                result.error(
                    f"conflicting source_path_id {source_id}: "
                    f"{source_ids[source_key]} vs {texture_id}")
            source_ids[source_key] = texture_id
        width = texture.get("width")
        height = texture.get("height")
        if not isinstance(width, int) or not isinstance(height, int) or width <= 0 or height <= 0:
            result.error(f"invalid texture dimensions: {texture_id}")
        # Scene metadata stores the serialized source format; the native
        # converter normalizes RGBA32/RGBA4444 to canonical RGBA8.
        if texture.get("format") not in ("RGBA32", "RGBA4444"):
            result.warn(f"unexpected source texture format: {texture_id} {texture.get('format')}")
        if require_generated:
            root = generated_root or (ROOT / "assets/generated/visual/r1_1_visual")
            generated = root / texture.get("png", "")
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

    result.counts = {
        "textures": len(scene.get("textures", [])),
        "sprites": len(scene.get("sprites", [])),
        "animations": len(scene.get("animations", [])),
        "characters": len(scene.get("characters", [])),
        "weapons": len(scene.get("weapons", [])),
        "enemies": len(scene.get("enemy_definitions", [])),
        "doors": len(scene.get("collision", {}).get("doors", [])),
        "obstacle_variants": len(scene.get("obstacles", {}).get("variants", [])),
    }

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


def _relative(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(ROOT))
    except ValueError:
        return str(path)


def write_reports(result: Audit, scene_path: Path, inventory_path: Path,
                  report_dir: Path) -> None:
    report_dir.mkdir(parents=True, exist_ok=True)
    status = "ERROR" if result.errors else ("WARNING" if result.warnings else "VALID")
    document = {
        "schema": 1,
        "tool": "tools/asset/audit_assets.py",
        "scene": _relative(scene_path),
        "inventory": _relative(inventory_path),
        "status": status,
        "counts": result.counts,
        "errors": result.errors,
        "warnings": result.warnings,
        "status_vocabulary": [
            "VALID", "WARNING", "ERROR", "UNRECOVERABLE", "UNKNOWN"
        ],
        "note": (
            "Reference-derived pixels are LOCAL_ONLY / NOT FOR REDISTRIBUTION. "
            "This report audits metadata and local generated files; it does not "
            "grant redistribution rights."
        ),
    }
    (report_dir / "audit-report.json").write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    lines = [
        "LiteKnight asset audit",
        f"scene: {document['scene']}",
        f"status: {status}",
        f"errors: {len(result.errors)}  warnings: {len(result.warnings)}",
    ]
    lines.extend(f"count {key}: {value}" for key, value in sorted(result.counts.items()))
    lines.extend(f"ERROR: {message}" for message in result.errors)
    lines.extend(f"WARNING: {message}" for message in result.warnings)
    lines.extend([
        "UNRECOVERABLE: categories with no native representation are listed in asset_inventory.json",
        "UNKNOWN: reference save format, UI flow and many MonoBehaviour behaviours",
        "local_only / reference_derived / not_for_redistribution",
    ])
    (report_dir / "audit-report.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scene", type=Path, default=DEFAULT_SCENE)
    parser.add_argument("--inventory", type=Path, default=DEFAULT_INVENTORY)
    parser.add_argument("--generated-root", type=Path,
                        default=ROOT / "assets/generated/visual/r1_1_visual")
    parser.add_argument("--report-dir", type=Path,
                        default=ROOT / "analysis/assets")
    parser.add_argument("--require-generated", action="store_true",
                        help="also fail when generated local texture PNGs are absent")
    args = parser.parse_args()
    result = audit(args.scene, args.inventory, args.require_generated,
                   args.generated_root)
    write_reports(result, args.scene, args.inventory, args.report_dir)
    print(f"LiteKnight asset audit: {args.scene}")
    print(f"  errors: {len(result.errors)}  warnings: {len(result.warnings)}")
    for message in result.errors:
        print(f"  ERROR: {message}")
    for message in result.warnings:
        print(f"  WARN:  {message}")
    print(f"  Reports: {args.report_dir}/audit-report.json/.txt")
    if result.errors:
        return 1
    print("  RESULT: metadata integrity PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
