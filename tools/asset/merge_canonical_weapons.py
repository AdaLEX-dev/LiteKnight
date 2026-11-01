#!/usr/bin/env python3
"""Merge the canonical recovered weapon/sprite catalogue into prepared rooms.

Weapon prefabs, bullets, summon sprites, audio ids and attack events live in the
shared `common` bundle. They are identical for every PatternRoom. Rebuilding all
108 room bundles with UnityPy for a shared-bundle-only change is unnecessary and
slow; this tool merges the canonical scene's weapon definitions and the few
common sprites/textures they reference into each prepared room while preserving
that room's geometry, doors, collision, enemies and spawn.
"""
from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CANONICAL = ROOT / "assets/generated/visual/r1_1_visual_tmp/scene.json"
DEFAULT_SCENE_ROOT = ROOT / "analysis/assets/scenes"
DEFAULT_GENERATED_ROOT = ROOT / "assets/generated/visual"


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def merge_room(canonical: dict, canonical_dir: Path, scene_path: Path,
               generated_dir: Path) -> tuple[int, int, int]:
    scene = load(scene_path)
    canonical_textures = {item["id"]: item for item in canonical["textures"]}
    target_textures = {item["id"]: item for item in scene["textures"]}
    canonical_sprites = {item["id"]: item for item in canonical["sprites"]}
    target_sprites = {item["id"]: item for item in scene["sprites"]}

    added_textures = 0
    for texture_id, texture in canonical_textures.items():
        if texture_id in target_textures:
            continue
        png = texture.get("png", "")
        source = canonical_dir / png
        destination = generated_dir / png
        if not source.is_file():
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.is_file():
            shutil.copy2(source, destination)
        target_textures[texture_id] = texture
        scene["textures"].append(texture)
        added_textures += 1

    added_sprites = 0
    for sprite_id, sprite in canonical_sprites.items():
        if sprite_id in target_sprites:
            continue
        if sprite.get("texture") not in target_textures:
            continue
        target_sprites[sprite_id] = sprite
        scene["sprites"].append(sprite)
        added_sprites += 1

    # Weapon definitions are shared catalogue data. Replace existing ids with
    # the canonical recovered definition and append newly recovered ids.
    existing = {weapon["id"]: index
                for index, weapon in enumerate(scene.get("weapons", []))}
    replaced = 0
    for weapon in canonical.get("weapons", []):
        index = existing.get(weapon["id"])
        if index is None:
            scene.setdefault("weapons", []).append(weapon)
            existing[weapon["id"]] = len(scene["weapons"]) - 1
            replaced += 1
        else:
            scene["weapons"][index] = weapon
            replaced += 1
    scene["weapons"].sort(key=lambda item: item["id"])
    scene["sprites"].sort(key=lambda item: item["id"])
    if canonical.get("level_themes"):
        scene["level_themes"] = canonical["level_themes"]
    scene_path.write_text(json.dumps(scene, indent=2, sort_keys=True) + "\n",
                          encoding="utf-8")
    return added_textures, added_sprites, replaced


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--canonical", type=Path, default=DEFAULT_CANONICAL)
    parser.add_argument("--scene-root", type=Path, default=DEFAULT_SCENE_ROOT)
    parser.add_argument("--generated-root", type=Path,
                        default=DEFAULT_GENERATED_ROOT)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    canonical_dir = args.canonical.parent
    canonical = load(args.canonical)
    rooms = 0
    for generated_dir in sorted(args.generated_root.glob("*_visual")):
        room = generated_dir.name
        if room == "hero_room":
            continue
        scene_path = generated_dir / "scene.json"
        if not scene_path.is_file():
            continue
        marker = generated_dir / ".weapons_merged"
        if marker.is_file() and not args.force:
            continue
        textures, sprites, weapons = merge_room(
            canonical, canonical_dir, scene_path, generated_dir)
        # Keep analysis metadata copies in sync without making them required.
        for metadata in (args.scene_root / f"{room}.json",
                         args.scene_root / "pattern" / f"{room}.json"):
            if metadata.is_file():
                merge_room(canonical, canonical_dir, metadata, generated_dir)
        marker.write_text("canonical recovered weapon catalogue merged\n",
                          encoding="utf-8")
        rooms += 1
        print(f"{room}: textures+{textures} sprites+{sprites} weapons={weapons}")
    print(f"Rooms merged: {rooms}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
