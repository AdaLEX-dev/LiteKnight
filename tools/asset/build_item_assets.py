#!/usr/bin/env python3
"""Extract the native chest/coin item pack from the actual reference prefabs.

The generated pixels remain local-only and gitignored. Metadata, sprite rects,
pivots and the evidence-derived coin values are committed under analysis/.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import shutil
from pathlib import Path

import UnityPy

ROOT = Path(__file__).resolve().parents[2]
BUNDLE = ROOT / "analysis/apktool_out/assets/AssetBundles/common"
OUT = ROOT / "assets/generated/items"
META = ROOT / "analysis/assets/items/item_assets.json"
MANIFEST = ROOT / "analysis/assets/items/item_assets_manifest.json"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def texture_name(sprite) -> str:
    return f"item_{sprite.m_RD.texture.path_id}".replace("-", "m")


def sprite_record(sprite, texture_id: str, name: str, texture_width: int,
                  texture_height: int, pivot_override=None) -> dict:
    rect = sprite.m_RD.textureRect
    offset = sprite.m_RD.textureRectOffset
    source_rect = sprite.m_Rect
    source_pivot = [float(sprite.m_Pivot.x), float(sprite.m_Pivot.y)]
    local_left = -source_pivot[0] * float(source_rect.width) + float(offset.x)
    local_bottom = -source_pivot[1] * float(source_rect.height) + float(offset.y)
    pivot = [
        -local_left / float(rect.width),
        -local_bottom / float(rect.height),
    ]
    if pivot_override is not None:
        pivot = list(pivot_override)
    pivot = [max(0.0, min(1.0, float(pivot[0]))),
             max(0.0, min(1.0, float(pivot[1])))]
    return {
        "id": name,
        "texture": texture_id,
        "source_name": str(getattr(sprite, "m_Name", "")),
        "rect": [float(rect.x), float(texture_height - rect.y - rect.height),
                 float(rect.width), float(rect.height)],
        "source_texture_rect": [float(rect.x), float(rect.y),
                                float(rect.width), float(rect.height)],
        "source_texture_rect_offset": [float(offset.x), float(offset.y)],
        "source_rect": [float(source_rect.x), float(source_rect.y),
                        float(source_rect.width), float(source_rect.height)],
        "source_pivot": source_pivot,
        "pivot": pivot,
        "pixels_per_unit": float(sprite.m_PixelsToUnits),
        "border": [0.0, 0.0, 0.0, 0.0],
        "filter": "point",
        "wrap": "clamp",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle", type=Path, default=BUNDLE)
    parser.add_argument("--out", type=Path, default=OUT)
    parser.add_argument("--meta", type=Path, default=META)
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    args = parser.parse_args()
    if not args.bundle.is_file():
        raise SystemExit(f"common bundle missing: {args.bundle}")
    # Sprite atlases are split across sibling bundles; load the complete local
    # extraction tree so external PPtrs resolve. Only the common bundle hash is
    # recorded as the item-prefab source.
    bundle_files = [str(args.bundle)]
    for path in sorted(args.bundle.parent.rglob("*")):
        if path.is_file() and not path.name.endswith(".manifest") and path != args.bundle:
            bundle_files.append(str(path))
    # The portal animation lives in the built-in level2 scene and its
    # sharedassets dependencies, not the common bundle.
    data_root = ROOT / "analysis/apktool_out/assets/bin/Data"
    for name in ("level2", "sharedassets2.assets",
                 "sharedassets2.assets.split0", "sharedassets2.assets.split1",
                 "sharedassets2.assets.split2", "sharedassets2.resource"):
        path = data_root / name
        if path.is_file():
            bundle_files.append(str(path))
    env = UnityPy.load(*bundle_files)

    game_objects = {}
    for obj in env.objects:
        if obj.type.name != "GameObject":
            continue
        try:
            data = obj.read()
        except Exception:
            continue
        name = str(getattr(data, "m_Name", ""))
        if name in {"chest00", "chest01", "chest02",
                    "coin_0", "coin_1", "coin_2"}:
            game_objects.setdefault(name, data)

    textures = {}
    texture_records = []
    sprites = []
    sprite_ids = {}
    sprite_assets = {}

    def ensure_texture(sprite) -> str:
        texture = sprite.m_RD.texture.read()
        text_id = texture_name(sprite)
        if text_id in textures:
            return text_id
        path = args.out / "textures" / f"{text_id}.png"
        path.parent.mkdir(parents=True, exist_ok=True)
        texture.image.save(path)
        textures[text_id] = (texture, path)
        texture_records.append({
            "id": text_id,
            "png": f"textures/{path.name}",
            "filter": "point",
            "wrap": "clamp",
        })
        return text_id

    def ensure_sprite(sprite_ptr, id_hint: str) -> str:
        sprite = sprite_ptr.read()
        key = str(sprite_ptr.path_id)
        if key in sprite_ids:
            return sprite_ids[key]
        texture = sprite.m_RD.texture.read()
        tex_id = ensure_texture(sprite)
        tex_w, tex_h = int(texture.m_Width), int(texture.m_Height)
        name = id_hint
        record = sprite_record(sprite, tex_id, name, tex_w, tex_h)
        sprites.append(record)
        sprite_ids[key] = name
        sprite_assets[name] = sprite
        return name

    chest_defs = []
    coin_defs = []
    for chest_name in ["chest00", "chest01", "chest02"]:
        go = game_objects.get(chest_name)
        if go is None:
            continue
        root = None
        for component in go.m_Component:
            if component.component.type.name == "Transform":
                root = component.component.read()
                break
        if root is None:
            raise SystemExit(f"{chest_name} has no Transform")
        visual_specs = []
        for child_ptr in getattr(root, "m_Children", []) or []:
            child = child_ptr.read()
            child_go = child.m_GameObject.read()
            for component in child_go.m_Component:
                if component.component.type.name != "SpriteRenderer":
                    continue
                renderer = component.component.read()
                if not renderer.m_Sprite or renderer.m_Sprite.path_id == 0:
                    continue
                sprite_id = ensure_sprite(renderer.m_Sprite,
                                          f"{chest_name}_{child_go.m_Name}")
                visual_specs.append({
                    "sprite": sprite_id,
                    "offset": [float(child.m_LocalPosition.x),
                               float(child.m_LocalPosition.y)],
                    "sorting_layer": int(renderer.m_SortingLayer),
                    "sorting_order": int(renderer.m_SortingOrder),
                })
        level = 0 if chest_name == "chest00" else int(chest_name[-1])
        chest_defs.append({
            "id": chest_name,
            "level": level,
            "visuals": visual_specs,
            "collider_half_extents": [0.5, 0.35],
            "collider_offset": [0.0, 0.35],
            "open_time": 1.0,
            "loot_coin": "",
            "loot_count": 0,
            "appointed_weapon": "weapon_021" if chest_name == "chest00" else "",
            "weapon_drop_level": level if chest_name != "chest00" else -1,
            "evidence": (
                "actual RGChestAppoint chest00 variant; one recovered appointed "
                "object_list entry is weapon_021" if chest_name == "chest00" else
                f"actual {chest_name} RGChest prefab level {level}; "
                "RGChest.OpenChest -> WeaponDropInfo.GetWeaponWithDropLevel"),
        })

    # Coin animation clips: the four PPtr frames are the actual spin sequence.
    clip_names = {"coin_0": "coin_gold", "coin_1": "coin_silver",
                  "coin_2": "coin_copper"}
    for coin_name in ["coin_0", "coin_1", "coin_2"]:
        go = game_objects.get(coin_name)
        if go is None:
            continue
        value = 5 if coin_name == "coin_0" else (3 if coin_name == "coin_1" else 1)
        coin_type = int(coin_name[-1])
        frames = []
        clip = None
        for obj in env.objects:
            if obj.type.name != "AnimationClip":
                continue
            try:
                candidate = obj.read()
            except Exception:
                continue
            if str(getattr(candidate, "m_Name", "")) == clip_names[coin_name]:
                clip = candidate
                break
        if clip is not None:
            for curve in clip.m_PPtrCurves:
                mapping = clip.m_ClipBindingConstant.pptrCurveMapping
                if hasattr(curve.curve, "keys"):
                    for key in curve.curve.keys:
                        ptr = key.value
                        if ptr.path_id != 0:
                            frames.append(ensure_sprite(ptr,
                                f"{coin_name}_frame{len(frames)}"))
                elif hasattr(curve.curve, "m_Curve"):
                    for key in curve.curve.m_Curve:
                        ptr = key.value
                        if ptr.path_id != 0:
                            frames.append(ensure_sprite(ptr,
                                f"{coin_name}_frame{len(frames)}"))
        if not frames and clip is not None:
            mapping = getattr(clip.m_ClipBindingConstant, "pptrCurveMapping", [])
            for ptr in mapping:
                if getattr(ptr, "path_id", 0) != 0:
                    frames.append(ensure_sprite(
                        ptr, f"{coin_name}_frame{len(frames)}"))
        if not frames:
            # Fallback to the prefab's single sprite; still actual reference art.
            for component in go.m_Component:
                if component.component.type.name != "Transform":
                    continue
                transform = component.component.read()
                for child_ptr in getattr(transform, "m_Children", []) or []:
                    child = child_ptr.read()
                    for comp in child.m_GameObject.read().m_Component:
                        if comp.component.type.name == "SpriteRenderer":
                            renderer = comp.component.read()
                            if renderer.m_Sprite and renderer.m_Sprite.path_id:
                                frames.append(ensure_sprite(
                                    renderer.m_Sprite, f"{coin_name}_frame0"))
        coin_defs.append({
            "id": coin_name,
            "type": coin_type,
            "value": value,
            "speed": 20.0,
            "range": 7.0,
            "frames": frames,
            "evidence": "RGCoin serialized type/value/speed/range; actual coin_gold/silver/copper clip frames where available",
        })

    # WeaponDropInfo is a serialized TextAsset in level2. RGChest.OpenChest
    # calls WeaponDropInfo.GetWeaponWithDropLevel(level, rg_random).
    weapon_drops = []
    drop_path = ROOT / "analysis/apktool_out/assets/bin/Data/45fdb92750c4d1c4ca9689110800ea48"
    if drop_path.is_file():
        drop_env = UnityPy.load(str(drop_path))
        for drop_obj in drop_env.objects:
            if drop_obj.type.name != "TextAsset":
                continue
            drop_data = drop_obj.read()
            if str(getattr(drop_data, "m_Name", "")) != "WeaponDropInfo":
                continue
            drop_doc = json.loads(drop_data.m_Script)
            for row in drop_doc.get("level2Weapon", []):
                weapon_drops.append({
                    "level": int(row.get("Key", 0)),
                    "entries": [
                        {"weapon": entry.get("name", ""),
                         "weight": int(entry.get("weight", 0)),
                         "title": entry.get("title", "")}
                        for entry in row.get("Value", [])
                        if entry.get("name")
                    ],
                })
            break

    portal_defs = []
    for obj in env.objects:
        if obj.type.name != "AnimationClip":
            continue
        try:
            clip = obj.read()
        except Exception:
            continue
        if str(getattr(clip, "m_Name", "")) != "transfer_gate_02":
            continue
        frames = []
        for ptr in getattr(clip.m_ClipBindingConstant, "pptrCurveMapping", []):
            if getattr(ptr, "path_id", 0) != 0:
                frames.append(ensure_sprite(ptr, f"portal_frame{len(frames)}"))
        if frames:
            portal_defs.append({
                "id": "transfer_gate_02",
                "frames": frames,
                "fps": float(getattr(clip, "m_SampleRate", 12.0)),
                "duration": float(clip.m_MuscleClip.m_StopTime),
                "loop": True,
                "evidence": "actual transfer_gate_02 AnimationClip sprite curve at 12 fps; level2 magic_protal2 assets",
            })
        break

    document = {
        "schema": 1,
        "tool": "tools/asset/build_item_assets.py",
        "source": "analysis/apktool_out/assets/AssetBundles/common",
        "camera": {"position": [0.0, 0.0], "orthographic_size": 10.5,
                   "viewport": [1920.0, 1080.0]},
        "spawn": [0.0, 0.0],
        "instances": [],
        "textures": texture_records,
        "sprites": sprites,
        "chests": chest_defs,
        "coins": coin_defs,
        "portals": portal_defs,
        "weapon_drops": weapon_drops,
        "notes": "Chest00/RGCoin prefab metadata/sprites plus the actual transfer_gate_02 portal sprite clip. Chest coin loot and lid animation are native test behavior; RGChest weapon selection and WeaponDropInfo are recovered in the 0.5.3 chest pipeline.",
    }
    args.meta.parent.mkdir(parents=True, exist_ok=True)
    args.meta.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n",
                         encoding="utf-8")
    manifest = {
        "schema": 1,
        "tool": "tools/asset/build_item_assets.py",
        "source_sha256": sha256(args.bundle),
        "meta_sha256": sha256(args.meta),
        "texture_count": len(texture_records),
        "sprite_count": len(sprites),
        "chest_count": len(chest_defs),
        "coin_count": len(coin_defs),
        "portal_count": len(portal_defs),
        "weapon_drop_levels": len(weapon_drops),
        "textures": [entry["id"] for entry in texture_records],
    }
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                             encoding="utf-8")
    print(f"Wrote {args.meta.relative_to(ROOT)}: {len(sprites)} sprites, "
          f"{len(chest_defs)} chests, {len(coin_defs)} coins")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
