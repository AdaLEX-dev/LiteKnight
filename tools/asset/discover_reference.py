#!/usr/bin/env python3
"""Scan the local reference bundles and build a complete metadata catalogue.

This is a discovery/inventory tool, not a pixel extractor. It records Unity
object identities and source bundle/CAB metadata so the asset laboratory can
track every recoverable category. No proprietary pixels are written.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from collections import Counter
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BUNDLE_ROOT = ROOT / "analysis/apktool_out/assets/AssetBundles"
DEFAULT_SCENE = ROOT / "analysis/assets/scenes/r1_1_visual.json"
DEFAULT_OUTPUT = ROOT / "analysis/assets/reference_catalogue.json"
EXPECTED_APK_SHA256 = "8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea"


def sha256_file(path: Path) -> str | None:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def classify_name(name: str) -> str | None:
    lower = name.lower()
    if lower.startswith(("ui_", "ui", "btn_", "button_", "text_", "img_ui")):
        return "ui"
    if any(token in lower for token in ("plant", "pot_", "garden")):
        return "plant"
    if any(token in lower for token in ("pickup", "drop_", "item_", "coin", "gem_")):
        return "item_pickup"
    if lower.startswith("texiao") or any(token in lower for token in ("effect", "explode", "boom", "fx_")):
        return "effect"
    if any(token in lower for token in ("tree", "grass", "stone", "deco", "prop_")):
        return "environment"
    if lower.startswith(("r1_", "room_")) or "door" in lower or "wall" in lower:
        return "room_object"
    return None


def build(bundle_root: Path, scene_path: Path, apk_path: Path) -> dict[str, Any]:
    import UnityPy  # imported lazily; require tools/asset/.venv

    scene = json.loads(scene_path.read_text(encoding="utf-8"))
    integrated_texture_ids = {str(t.get("source_path_id")) for t in scene.get("textures", [])}
    integrated_sprite_ids = {str(s.get("sprite_path_id")) for s in scene.get("sprites", [])}
    integrated_character_ids = {str(c.get("id")) for c in scene.get("characters", [])}
    integrated_weapon_ids = {str(w.get("id")) for w in scene.get("weapons", [])}
    integrated_enemy_ids = {str(e.get("id")) for e in scene.get("enemy_definitions", [])}
    integrated_animation_ids = {Path(a).stem for a in scene.get("animations", [])}

    files = [p for p in bundle_root.rglob("*") if p.is_file()]
    env = UnityPy.load(*[str(path) for path in files])

    textures = []
    sprites = []
    animations = []
    characters = []
    weapons = []
    enemies = []
    prefab_names: Counter[str] = Counter()
    category_names: dict[str, Counter[str]] = {
        "ui": Counter(), "plant": Counter(), "item_pickup": Counter(),
        "effect": Counter(), "environment": Counter(), "room_object": Counter(),
    }
    component_counts: Counter[str] = Counter()

    for obj in env.objects:
        type_name = obj.type.name
        component_counts[type_name] += 1
        if type_name == "Texture2D":
            try:
                data = obj.read()
            except Exception:
                continue
            textures.append({
                "name": getattr(data, "m_Name", ""),
                "path_id": str(obj.path_id),
                "cab": getattr(obj.assets_file, "name", ""),
                "width": int(getattr(data, "m_Width", 0)),
                "height": int(getattr(data, "m_Height", 0)),
                "format": int(getattr(data, "m_TextureFormat", -1)),
                "status": "INTEGRATED" if str(obj.path_id) in integrated_texture_ids else "DISCOVERED",
            })
        elif type_name == "Sprite":
            try:
                data = obj.read()
            except Exception:
                continue
            sprite_id = str(obj.path_id)
            rect = getattr(data, "m_Rect", None)
            pivot = getattr(data, "m_Pivot", None)
            texture_ptr = getattr(getattr(data, "m_RD", None), "texture", None)
            sprites.append({
                "name": getattr(data, "m_Name", ""),
                "path_id": sprite_id,
                "cab": getattr(obj.assets_file, "name", ""),
                "texture_path_id": str(getattr(texture_ptr, "path_id", "")),
                "rect": [float(getattr(rect, "x", 0.0)), float(getattr(rect, "y", 0.0)),
                         float(getattr(rect, "width", 0.0)), float(getattr(rect, "height", 0.0))] if rect else None,
                "pivot": [float(getattr(pivot, "x", 0.0)), float(getattr(pivot, "y", 0.0))] if pivot else None,
                "pixels_per_unit": float(getattr(data, "m_PixelsToUnits", 0.0)),
                "status": "INTEGRATED" if sprite_id in integrated_sprite_ids else "DISCOVERED",
            })
        elif type_name == "AnimationClip":
            try:
                data = obj.read()
            except Exception:
                continue
            name = getattr(data, "m_Name", "")
            animations.append({
                "name": name,
                "path_id": str(obj.path_id),
                "cab": getattr(obj.assets_file, "name", ""),
                "sample_rate": float(getattr(data, "m_SampleRate", 0.0)),
                "status": "INTEGRATED" if name in integrated_animation_ids else "DISCOVERED",
            })
        elif type_name == "GameObject":
            try:
                game_object = obj.read()
            except Exception:
                continue
            name = getattr(game_object, "m_Name", "")
            prefab_names[name] += 1
            category = classify_name(name)
            if category:
                category_names[category][name] += 1
            if re.fullmatch(r"c(0[0-9]|1[0-4])", name):
                role = None
                for component in game_object.m_Component:
                    if component.component.type.name != "MonoBehaviour":
                        continue
                    try:
                        candidate = component.component.read()
                    except Exception:
                        continue
                    if hasattr(candidate, "max_armor") and hasattr(candidate, "max_hp"):
                        role = candidate
                characters.append({
                    "id": name,
                    "path_id": str(obj.path_id),
                    "cab": getattr(obj.assets_file, "name", ""),
                    "max_hp": int(getattr(role, "max_hp", 0)) if role else None,
                    "max_armor": int(getattr(role, "max_armor", 0)) if role else None,
                    "max_energy": int(getattr(role, "max_energy", 0)) if role else None,
                    "speed": float(getattr(role, "speed", 0.0)) if role else None,
                    "status": "INTEGRATED" if name in integrated_character_ids else "DISCOVERED",
                })
            elif name.startswith("weapon_"):
                gun = None
                for component in game_object.m_Component:
                    if component.component.type.name != "MonoBehaviour":
                        continue
                    try:
                        candidate = component.component.read()
                    except Exception:
                        continue
                    if hasattr(candidate, "bulletsInfo") and candidate.bulletsInfo:
                        gun = candidate
                if gun is not None:
                    info = gun.bulletsInfo[0]
                    weapons.append({
                        "id": name,
                        "path_id": str(obj.path_id),
                        "cab": getattr(obj.assets_file, "name", ""),
                        "projectile_path_id": str(getattr(info.bulletProto, "path_id", "")),
                        "damage": int(getattr(info, "damage", 0)),
                        "speed": float(getattr(info, "speed", 0.0)),
                        "deviation": int(getattr(gun, "deviation", 0)),
                        "consume": int(getattr(gun, "consume", 0)),
                        "weapon_speed": float(getattr(gun, "weapon_speed", 1.0)),
                        "status": "INTEGRATED" if name in integrated_weapon_ids else "DISCOVERED",
                    })
            else:
                for component in game_object.m_Component:
                    if component.component.type.name != "MonoBehaviour":
                        continue
                    try:
                        candidate = component.component.read()
                    except Exception:
                        continue
                    enemy_id = getattr(candidate, "enemy_id", None)
                    if enemy_id:
                        enemies.append({
                            "id": str(enemy_id),
                            "game_object": name,
                            "path_id": str(obj.path_id),
                            "cab": getattr(obj.assets_file, "name", ""),
                            "status": "INTEGRATED" if str(enemy_id) in integrated_enemy_ids else "DISCOVERED",
                        })
                        break

    def summarize(items: list[dict[str, Any]], id_key: str = "id") -> dict[str, int]:
        counts = Counter(item.get("status", "UNKNOWN") for item in items)
        unique = {str(item.get(id_key, item.get("path_id", ""))) for item in items}
        unique_integrated = {
            str(item.get(id_key, item.get("path_id", "")))
            for item in items if item.get("status") == "INTEGRATED"
        }
        return {
            "total": len(items),
            "unique_total": len(unique),
            "unique_integrated": len(unique_integrated),
            "unique_discovered": len(unique) - len(unique_integrated),
            **dict(sorted(counts.items())),
        }

    category_summary = {
        category: {"unique_names": len(names), "instances": sum(names.values())}
        for category, names in category_names.items()
    }
    catalogue = {
        "schema": 1,
        "tool": "tools/asset/discover_reference.py",
        "reference_apk": str(apk_path.relative_to(ROOT)),
        "reference_apk_sha256": sha256_file(apk_path),
        "expected_apk_sha256": EXPECTED_APK_SHA256,
        "bundle_root": str(bundle_root.relative_to(ROOT)),
        "bundle_file_count": len(files),
        "scene": str(scene_path.relative_to(ROOT)),
        "summary": {
            "textures": summarize(textures),
            "sprites": summarize(sprites),
            "animations": summarize(animations),
            "characters": summarize(characters, "id"),
            "weapons": summarize(weapons, "id"),
            "enemies": summarize(enemies, "id"),
            "game_objects": len(prefab_names),
            "component_counts": dict(sorted(component_counts.items())),
            "name_heuristic_categories": category_summary,
        },
        "textures": textures,
        "sprites": sprites,
        "animations": animations,
        "characters": characters,
        "weapons": weapons,
        "enemies": enemies,
        "game_object_names": [
            {"name": name, "instances": count}
            for name, count in sorted(prefab_names.items())
        ],
        "notes": [
            "DISCOVERED = present in local reference bundles, not yet translated.",
            "INTEGRATED = referenced by the current native scene metadata.",
            "Name-heuristic categories are discovery aids, not confirmed gameplay semantics.",
            "All reference-derived content is local_only / not for redistribution.",
        ],
    }
    return catalogue


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle-root", type=Path, default=DEFAULT_BUNDLE_ROOT)
    parser.add_argument("--scene", type=Path, default=DEFAULT_SCENE)
    parser.add_argument("--apk", type=Path, default=ROOT / "original/SoulKnight-2.6.8.apk")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    catalogue = build(args.bundle_root, args.scene, args.apk)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(catalogue, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Wrote {args.output.relative_to(ROOT)}")
    print(json.dumps(catalogue["summary"], indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
