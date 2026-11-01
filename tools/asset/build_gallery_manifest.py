#!/usr/bin/env python3
"""Build the reference texture/sprite/prefab gallery manifest.

The gallery executable is a development asset laboratory. This tool enumerates
every prepared visual asset from the generated native scene/item/UI packs and
validates texture paths, sprite rectangles, pivots, PPU and prefab parts.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_ASSET_ROOT = ROOT / "build/assets"
DEFAULT_OUTPUT = ROOT / "analysis/assets/reference_gallery_manifest.json"


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def finite(value) -> bool:
    return isinstance(value, (int, float)) and math.isfinite(float(value))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset-root", type=Path, default=DEFAULT_ASSET_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    asset_root = args.asset_root
    output = args.output
    if not asset_root.is_dir():
        raise SystemExit(f"asset root missing: {asset_root}")

    textures: dict[str, dict] = {}
    sprites: dict[str, dict] = {}
    prefabs: dict[str, dict] = {}
    entries: list[dict] = []
    broken: list[dict] = []
    scene_documents: list[tuple[str, Path, dict]] = []

    def add_texture(key, name, source, scene, rel_path, width=0, height=0,
                    filter_name="point", wrap="clamp"):
        if key in textures:
            return key
        path = asset_root / rel_path
        if not path.is_file():
            textures[key] = {
                "key": key, "name": name, "source": source, "scene": scene,
                "path": rel_path, "width": width, "height": height,
                "filter": filter_name, "wrap": wrap,
                "status": "broken", "reason": "PNG missing",
            }
            broken.append({"type": "texture", "key": key,
                           "reason": f"PNG missing: {rel_path}"})
            return key
        try:
            from PIL import Image
            image = Image.open(path)
            image.verify()
            image = Image.open(path).convert("RGBA")
            width, height = image.size
        except Exception as exc:
            textures[key] = {
                "key": key, "name": name, "source": source, "scene": scene,
                "path": rel_path, "width": width, "height": height,
                "filter": filter_name, "wrap": wrap,
                "status": "broken", "reason": f"PNG decode failed: {exc}",
            }
            broken.append({"type": "texture", "key": key,
                           "reason": f"PNG decode failed: {exc}"})
            return key
        textures[key] = {
            "key": key, "name": name, "source": source, "scene": scene,
            "path": rel_path, "width": int(width), "height": int(height),
            "filter": str(filter_name).lower(), "wrap": str(wrap).lower(),
            "status": "included",
        }
        return key

    def add_sprite(key, name, scene, texture_key, rect, pivot, ppu,
                   source_name=""):
        if key in sprites:
            return key
        status = "included"
        reason = ""
        if texture_key not in textures:
            status, reason = "broken", "texture key missing"
        else:
            tex = textures[texture_key]
            if tex["status"] != "included":
                status, reason = "broken", "texture broken"
            elif (not isinstance(rect, list) or len(rect) != 4 or
                  not all(finite(v) for v in rect)):
                status, reason = "broken", "sprite rect invalid"
            elif (rect[2] <= 0 or rect[3] <= 0 or rect[0] < 0 or
                  rect[1] < 0 or rect[0] + rect[2] > tex["width"] + 0.01 or
                  rect[1] + rect[3] > tex["height"] + 0.01):
                status, reason = "broken", "sprite rect outside texture"
            elif (not isinstance(pivot, list) or len(pivot) != 2 or
                  not all(finite(v) and 0.0 <= v <= 1.0 for v in pivot)):
                status, reason = "broken", "sprite pivot invalid"
            elif not finite(ppu) or float(ppu) <= 0:
                status, reason = "broken", "sprite PPU invalid"
        sprites[key] = {
            "key": key, "name": name, "scene": scene,
            "texture_key": texture_key, "rect": rect, "pivot": pivot,
            "ppu": float(ppu) if finite(ppu) else 0.0,
            "source_name": source_name, "status": status,
            **({"reason": reason} if reason else {}),
        }
        if status != "included":
            broken.append({"type": "sprite", "key": key, "reason": reason})
        return key

    def add_prefab(key, name, prefab_type, scene, source, parts):
        if key in prefabs:
            return key
        valid_parts = []
        status = "included"
        reason = ""
        for part in parts:
            sprite_key = part.get("sprite_key")
            if sprite_key not in sprites:
                status = "broken"
                reason = f"prefab sprite missing: {sprite_key}"
                continue
            offset = part.get("offset", [0.0, 0.0])
            scale = part.get("scale", [1.0, 1.0])
            if (not isinstance(offset, list) or len(offset) != 2 or
                    not all(finite(v) for v in offset) or
                    not isinstance(scale, list) or len(scale) != 2 or
                    not all(finite(v) for v in scale)):
                status = "broken"
                reason = "prefab transform invalid"
                continue
            valid_parts.append({
                "sprite_key": sprite_key,
                "offset": offset,
                "scale": scale,
                "rotation": float(part.get("rotation", 0.0)),
                "sorting_layer": int(part.get("sorting_layer", 5)),
                "sorting_order": int(part.get("sorting_order", 0)),
                "flip_x": bool(part.get("flip_x", False)),
                "flip_y": bool(part.get("flip_y", False)),
            })
        prefabs[key] = {
            "key": key, "name": name, "type": prefab_type, "scene": scene,
            "source": source, "parts": valid_parts, "status": status,
            **({"reason": reason} if reason else {}),
        }
        if status != "included":
            broken.append({"type": "prefab", "key": key, "reason": reason})
        return key

    # Native scene packs.
    scene_root = asset_root / "scenes"
    for scene_json in sorted(scene_root.glob("*/scene.json")):
        scene_id = scene_json.parent.name
        try:
            document = read_json(scene_json)
        except Exception as exc:
            broken.append({"type": "scene", "key": scene_id,
                           "reason": f"scene JSON failed: {exc}"})
            continue
        scene_documents.append((scene_id, scene_json.parent, document))
        texture_keys = {}
        for tex in document.get("textures", []):
            key = "tex:" + str(tex.get("source_path_id") or tex.get("id"))
            rel = f"scenes/{scene_id}/{tex.get('png', '')}"
            add_texture(key, tex.get("source_name") or tex.get("id", key),
                        "scene", scene_id, rel, tex.get("width", 0),
                        tex.get("height", 0), tex.get("filter", "point"),
                        tex.get("wrap", "clamp"))
            texture_keys[tex.get("id")] = key
        sprite_keys = {}
        for sprite in document.get("sprites", []):
            key = "spr:" + str(sprite.get("sprite_path_id") or
                               f"{scene_id}:{sprite.get('id')}")
            texture_key = texture_keys.get(sprite.get("texture"))
            add_sprite(key, sprite.get("source_name") or sprite.get("id", key),
                       scene_id, texture_key, sprite.get("rect"),
                       sprite.get("pivot"), sprite.get("pixels_per_unit"),
                       sprite.get("source_name", ""))
            sprite_keys[sprite.get("id")] = key

        # Door prefabs: recover leaf + shadow + ornament as one visual group.
        for instance in document.get("instances", []):
            door = instance.get("door")
            if not door:
                continue
            role = instance.get("door_role", "leaf")
            door_key = f"prefab:door:{scene_id}:{door}:{role}"
            add_prefab(door_key, f"door {door} {role}", "door", scene_id,
                       instance.get("path", ""), [{
                           "sprite_key": sprite_keys.get(instance.get("sprite")),
                           "offset": [0.0, 0.0],
                           "scale": instance.get("scale", [1.0, 1.0]),
                           "rotation": instance.get("rotation", 0.0),
                           "sorting_layer": instance.get("sorting_layer", 5),
                           "sorting_order": instance.get("sorting_order", 0),
                           "flip_x": instance.get("flip_x", False),
                           "flip_y": instance.get("flip_y", False),
                       }])

        if scene_id == "r1_1_visual":
            for weapon in document.get("weapons", []):
                parts = []
                if weapon.get("visual_sprite") in sprite_keys:
                    parts.append({
                        "sprite_key": sprite_keys[weapon["visual_sprite"]],
                        "offset": weapon.get("visual_offset", [0.0, 0.0]),
                        "scale": [weapon.get("visual_scale", 1.0)] * 2,
                        "rotation": weapon.get("visual_rotation", 0.0),
                        "sorting_layer": weapon.get("sorting_layer", 5),
                        "sorting_order": weapon.get("sorting_order", 1),
                    })
                add_prefab("prefab:weapon:" + str(weapon.get("id")),
                           str(weapon.get("id")), "weapon", scene_id,
                           "RGWeapon prefab", parts)
            for enemy in document.get("enemy_definitions", []):
                parts = []
                for field, layer in (("body_sprite", 5),
                                     ("dead_sprite", 5),
                                     ("shadow_sprite", 3)):
                    sid = enemy.get(field)
                    if sid in sprite_keys:
                        parts.append({
                            "sprite_key": sprite_keys[sid],
                            "offset": [0.0, 0.0],
                            "scale": [1.0, 1.0],
                            "sorting_layer": layer,
                            "sorting_order": 1 if layer == 5 else 0,
                        })
                add_prefab("prefab:enemy:" + str(enemy.get("id")),
                           str(enemy.get("id")), "enemy", scene_id,
                           "RGEController prefab", parts)

        if scene_id == "hero_room":
            for character in document.get("characters", []):
                parts = []
                for sid in (character.get("body_sprite"),
                            character.get("dead_sprite")):
                    if sid in sprite_keys:
                        parts.append({
                            "sprite_key": sprite_keys[sid],
                            "offset": [0.0, 0.0], "scale": [1.0, 1.0],
                            "sorting_layer": 5, "sorting_order": 1,
                        })
                add_prefab("prefab:character:" + str(character.get("id")),
                           str(character.get("id")), "character", scene_id,
                           character.get("controller_name", ""), parts)
            for pet in document.get("pet_definitions", []):
                parts = []
                for sid in (pet.get("body_sprite"),
                            pet.get("shadow_sprite")):
                    if sid in sprite_keys:
                        parts.append({
                            "sprite_key": sprite_keys[sid],
                            "offset": [0.0, 0.0], "scale": [1.0, 1.0],
                            "sorting_layer": 5, "sorting_order": 1,
                        })
                add_prefab("prefab:pet:" + str(pet.get("id")),
                           str(pet.get("id")), "pet", scene_id,
                           pet.get("class_name", ""), parts)

        # Obstacle prefab variants are the actual room prefab visuals.
        for variant in document.get("obstacles", {}).get("variants", []):
            key = "prefab:obstacle:" + str(variant.get("path_id") or
                                           variant.get("name"))
            parts = []
            for visual in variant.get("visuals", []):
                sid = visual.get("sprite")
                if sid in sprite_keys:
                    parts.append({
                        "sprite_key": sprite_keys[sid],
                        "offset": visual.get("offset", [0.0, 0.0]),
                        "scale": visual.get("scale", [1.0, 1.0]),
                        "rotation": visual.get("rotation", 0.0),
                        "sorting_layer": visual.get("sorting_layer", 5),
                        "sorting_order": visual.get("sorting_order", 0),
                        "flip_x": visual.get("flip_x", False),
                        "flip_y": visual.get("flip_y", False),
                    })
            add_prefab(key, str(variant.get("name")), "obstacle", scene_id,
                       "MapManagerLevel.obstacle_list", parts)

    # Item pack (chests, coins, materials, energy, portal).
    item_json = scene_root / "items" / "items.json"
    if item_json.is_file():
        try:
            items = read_json(item_json)
        except Exception as exc:
            broken.append({"type": "items", "key": "items",
                           "reason": str(exc)})
            items = {}
        tex_keys = {}
        for tex in items.get("textures", []):
            key = "tex:" + str(tex.get("source_path_id") or tex.get("id"))
            add_texture(key, tex.get("source_name") or tex.get("id", key),
                        "items", "items",
                        f"scenes/items/{tex.get('png', '')}",
                        tex.get("width", 0), tex.get("height", 0),
                        tex.get("filter", "point"), tex.get("wrap", "clamp"))
            tex_keys[tex.get("id")] = key
        sprite_keys = {}
        for sprite in items.get("sprites", []):
            key = "spr:item:" + str(sprite.get("id"))
            add_sprite(key, sprite.get("source_name") or sprite.get("id", key),
                       "items", tex_keys.get(sprite.get("texture")),
                       sprite.get("rect"), sprite.get("pivot"),
                       sprite.get("pixels_per_unit"),
                       sprite.get("source_name", ""))
            sprite_keys[sprite.get("id")] = key
        for chest in items.get("chests", []):
            parts = []
            for visual in chest.get("visuals", []):
                sid = visual.get("sprite")
                if sid in sprite_keys:
                    parts.append({
                        "sprite_key": sprite_keys[sid],
                        "offset": visual.get("offset", [0.0, 0.0]),
                        "scale": [1.0, 1.0],
                        "sorting_layer": visual.get("sorting_layer", 5),
                        "sorting_order": visual.get("sorting_order", 0),
                    })
            add_prefab("prefab:chest:" + str(chest.get("id")),
                       str(chest.get("id")), "chest", "items",
                       chest.get("evidence", ""), parts)
        for coin in items.get("coins", []):
            frames = coin.get("frames", [])
            parts = []
            if frames and frames[0] in sprite_keys:
                parts.append({"sprite_key": sprite_keys[frames[0]],
                              "offset": [0.0, 0.0],
                              "scale": [1.0, 1.0]})
            add_prefab("prefab:coin:" + str(coin.get("id")),
                       str(coin.get("id")), "coin", "items",
                       "RGCoin prefab", parts)
        for material in items.get("materials", []):
            sid = material.get("sprite")
            parts = ([{"sprite_key": sprite_keys[sid],
                       "offset": material.get("offset", [0.0, 0.0]),
                       "scale": [1.0, 1.0]}] if sid in sprite_keys else [])
            add_prefab("prefab:material:" + str(material.get("id")),
                       str(material.get("id")), "material", "items",
                       "ItemPickable material prefab", parts)
        energy = items.get("energy")
        if isinstance(energy, dict) and energy.get("sprite") in sprite_keys:
            add_prefab("prefab:energy", "energy", "pickup", "items",
                       "RGEnergy prefab", [{
                           "sprite_key": sprite_keys[energy["sprite"]],
                           "offset": energy.get("offset", [0.0, 0.0]),
                           "scale": [1.0, 1.0]}])
        for portal in items.get("portals", []):
            frames = portal.get("frames", [])
            parts = ([{"sprite_key": sprite_keys[frames[0]],
                       "offset": [0.0, 0.0], "scale": [1.0, 1.0]}]
                     if frames and frames[0] in sprite_keys else [])
            add_prefab("prefab:portal:" + str(portal.get("id")),
                       str(portal.get("id")), "portal", "items",
                       "transfer_gate_02 prefab", parts)

    # UI pack.
    ui_json = asset_root / "ui" / "manifest.json"
    if ui_json.is_file():
        try:
            ui = read_json(ui_json)
        except Exception as exc:
            broken.append({"type": "ui", "key": "ui", "reason": str(exc)})
            ui = {}
        tex_keys = {}
        for tex in ui.get("textures", []):
            key = "tex:ui:" + str(tex.get("id"))
            add_texture(key, tex.get("name") or tex.get("id", key), "ui", "ui",
                        f"ui/{tex.get('png', '')}", tex.get("width", 0),
                        tex.get("height", 0), tex.get("filter", "point"),
                        tex.get("wrap", "clamp"))
            tex_keys[tex.get("id")] = key
        sprite_keys = {}
        for sprite in ui.get("sprites", []):
            key = "spr:ui:" + str(sprite.get("id"))
            rect = sprite.get("rect_pixels", [0.0, 0.0, 0.0, 0.0])
            add_sprite(key, sprite.get("name") or sprite.get("id", key),
                       "ui", tex_keys.get(sprite.get("texture")), rect,
                       sprite.get("pivot"), sprite.get("pixels_per_unit"),
                       sprite.get("name", ""))
            sprite_keys[sprite.get("id")] = key
            add_prefab("prefab:ui:" + str(sprite.get("id")),
                       str(sprite.get("name") or sprite.get("id")), "ui", "ui",
                       "UIAssets sprite", [{
                           "sprite_key": key, "offset": [0.0, 0.0],
                           "scale": [1.0, 1.0]}])

    # Stable deterministic order and entry list.
    for key in sorted(textures):
        entries.append({"type": "texture", "key": key})
    for key in sorted(sprites):
        entries.append({"type": "sprite", "key": key})
    for key in sorted(prefabs):
        entries.append({"type": "prefab", "key": key})

    document = {
        "schema": 1,
        "tool": "tools/asset/build_gallery_manifest.py",
        "asset_root": str(asset_root),
        "counts": {
            "textures": len(textures),
            "sprites": len(sprites),
            "prefabs": len(prefabs),
            "entries": len(entries),
            "broken": len(broken),
            "scenes": len(scene_documents),
        },
        "textures": [textures[k] for k in sorted(textures)],
        "sprites": [sprites[k] for k in sorted(sprites)],
        "prefabs": [prefabs[k] for k in sorted(prefabs)],
        "entries": entries,
        "broken": broken,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {output}: {len(entries)} entries "
          f"({len(textures)} textures, {len(sprites)} sprites, "
          f"{len(prefabs)} prefabs), broken={len(broken)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
