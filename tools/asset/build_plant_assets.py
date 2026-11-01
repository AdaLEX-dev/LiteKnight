#!/usr/bin/env python3
"""Extract the recovered Living Room plant prefabs into the item pack.

The reference `plant_*` prefabs live in the common bundle. Each prefab has a
`state_root/state_N/img` SpriteRenderer hierarchy and a sibling
`plant_*_seed` ItemPickable with the actual seed icon. This tool appends those
actual sprites and serialized plant fields to `analysis/assets/items/
item_assets.json`; it never invents growth art or plant names.

Growth duration is not serialized in the recovered prefab. The runtime uses a
marked native reconstruction timer (30 s) until the authoritative `MechInfos`/
`PlantInfo` resource table is recovered.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

import UnityPy

ROOT = Path(__file__).resolve().parents[2]
BUNDLE = ROOT / "analysis/apktool_out/assets/AssetBundles/common"
META = ROOT / "analysis/assets/items/item_assets.json"
TEXTURE_OUT = ROOT / "assets/generated/items/textures"
LOCALIZATION = ROOT / "analysis/assets/localization.json"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def texture_id(sprite) -> str:
    return f"item_{sprite.m_RD.texture.path_id}".replace("-", "m")


def sprite_record(sprite, texture: str, name: str, texture_width: int,
                  texture_height: int) -> dict:
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
    pivot = [max(0.0, min(1.0, float(pivot[0]))),
             max(0.0, min(1.0, float(pivot[1])))]
    return {
        "id": name,
        "texture": texture,
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


def transform_of(game_object):
    for component in game_object.m_Component:
        if component.component.type.name == "Transform":
            return component.component.read()
    return None


def sprite_renderer_of(game_object):
    for component in game_object.m_Component:
        if component.component.type.name == "SpriteRenderer":
            try:
                return component.component.read()
            except Exception:
                return None
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle", type=Path, default=BUNDLE)
    parser.add_argument("--meta", type=Path, default=META)
    parser.add_argument("--textures", type=Path, default=TEXTURE_OUT)
    args = parser.parse_args()
    if not args.bundle.is_file():
        raise SystemExit(f"common bundle missing: {args.bundle}")
    if not args.meta.is_file():
        raise SystemExit(f"item metadata missing: {args.meta}")

    bundle_files = [str(args.bundle)]
    for path in sorted(args.bundle.parent.rglob("*")):
        if path.is_file() and not path.name.endswith(".manifest") and path != args.bundle:
            bundle_files.append(str(path))
    data_root = ROOT / "analysis/apktool_out/assets/bin/Data"
    for name in ("level2", "sharedassets2.assets",
                 "sharedassets2.assets.split0", "sharedassets2.assets.split1",
                 "sharedassets2.assets.split2", "sharedassets2.resource"):
        path = data_root / name
        if path.is_file():
            bundle_files.append(str(path))
    env = UnityPy.load(*bundle_files)

    game_objects = {}
    transform_to_go = {}
    for obj in env.objects:
        if obj.type.name != "GameObject":
            continue
        try:
            data = obj.read()
        except Exception:
            continue
        game_objects[obj.path_id] = data
        for component in data.m_Component:
            if component.component.type.name == "Transform":
                transform_to_go[component.component.path_id] = obj.path_id

    document = json.loads(args.meta.read_text(encoding="utf-8"))
    sprites = document.setdefault("sprites", [])
    textures = document.setdefault("textures", [])
    sprite_ids = {entry["id"] for entry in sprites}
    texture_ids = {entry["id"] for entry in textures}
    args.textures.mkdir(parents=True, exist_ok=True)

    localized = {}
    if LOCALIZATION.is_file():
        localized = json.loads(
            LOCALIZATION.read_text(encoding="utf-8")).get("terms", {})

    def ensure_texture(sprite) -> str:
        tex = sprite.m_RD.texture.read()
        ident = texture_id(sprite)
        if ident in texture_ids:
            return ident
        path = args.textures / f"{ident}.png"
        tex.image.save(path)
        textures.append({
            "id": ident,
            "png": f"textures/{path.name}",
            "filter": "point",
            "wrap": "clamp",
        })
        texture_ids.add(ident)
        return ident

    def ensure_sprite(sprite, name: str) -> str:
        if name in sprite_ids:
            return name
        tex = sprite.m_RD.texture.read()
        tex_id = ensure_texture(sprite)
        sprites.append(sprite_record(sprite, tex_id, name,
                                     int(tex.m_Width), int(tex.m_Height)))
        sprite_ids.add(name)
        return name

    def child_named(game_object, name: str):
        transform = transform_of(game_object)
        if transform is None:
            return None
        for child_ptr in getattr(transform, "m_Children", []) or []:
            child_transform = child_ptr.read()
            child = child_transform.m_GameObject.read()
            if child.m_Name == name:
                return child, child_transform
        return None

    plant_names = sorted(
        name for name in {obj.m_Name for obj in game_objects.values()}
        if re.fullmatch(r"plant_[a-z_]+", name) and not name.endswith("_seed"))

    plants = []
    for plant_name in plant_names:
        root = next((obj for obj in game_objects.values()
                     if obj.m_Name == plant_name), None)
        if root is None:
            continue
        root_transform = transform_of(root)
        state_root_hit = child_named(root, "state_root")
        if root_transform is None or state_root_hit is None:
            continue
        state_root, state_root_transform = state_root_hit
        states = []
        for state_index in range(16):
            state_hit = child_named(state_root, f"state_{state_index}")
            if state_hit is None:
                continue
            state_go, state_transform = state_hit
            image_go = None
            image_transform = None
            for child_ptr in getattr(state_transform, "m_Children", []) or []:
                child_transform = child_ptr.read()
                child = child_transform.m_GameObject.read()
                if child.m_Name == "img":
                    image_go = child
                    image_transform = child_transform
                    break
            if image_go is None:
                continue
            renderer = sprite_renderer_of(image_go)
            if renderer is None or renderer.m_Sprite is None:
                continue
            if not bool(getattr(renderer, "m_Enabled", True)):
                continue
            try:
                sprite = renderer.m_Sprite.read()
            except Exception:
                continue
            sprite_name = ensure_sprite(
                sprite, f"{plant_name}_state_{state_index}")
            offset = [
                float(getattr(state_root_transform, "m_LocalPosition").x) +
                float(getattr(state_transform, "m_LocalPosition").x) +
                float(getattr(image_transform, "m_LocalPosition").x),
                float(getattr(state_root_transform, "m_LocalPosition").y) +
                float(getattr(state_transform, "m_LocalPosition").y) +
                float(getattr(image_transform, "m_LocalPosition").y),
            ]
            states.append({
                "state": state_index,
                "sprite": sprite_name,
                "offset": offset,
                "sorting_layer": int(renderer.m_SortingLayer),
                "sorting_order": int(renderer.m_SortingOrder),
            })
        if not states:
            continue

        seed_root = next((obj for obj in game_objects.values()
                          if obj.m_Name == plant_name + "_seed"), None)
        seed_sprite = ""
        if seed_root is not None:
            for candidate_name in ("i", "icon"):
                hit = child_named(seed_root, candidate_name)
                if hit is None:
                    continue
                candidate, _ = hit
                renderer = sprite_renderer_of(candidate)
                if renderer is None or renderer.m_Sprite is None:
                    continue
                try:
                    seed_sprite = ensure_sprite(
                        renderer.m_Sprite.read(), f"{plant_name}_seed_icon")
                    break
                except Exception:
                    continue

        class_name = ""
        item_name = ""
        item_value = 0
        harvest_id = ""
        harvest_count = 1
        for component in root.m_Component:
            if component.component.type.name != "MonoBehaviour":
                continue
            try:
                candidate = component.component.read()
                script = candidate.m_Script.read()
                class_name = str(getattr(script, "m_ClassName", "") or "")
                item_name = str(getattr(candidate, "item_name", "") or "")
                item_value = int(getattr(candidate, "item_value", 0) or 0)
                harvest_count = max(1, int(getattr(candidate, "count", 1) or 1))
                for field in ("weapon", "material", "item"):
                    pointer = getattr(candidate, field, None)
                    if pointer is None or getattr(pointer, "path_id", 0) == 0:
                        continue
                    try:
                        target = pointer.read()
                        if hasattr(target, "m_Name"):
                            harvest_id = str(target.m_Name)
                    except Exception:
                        pass
                break
            except Exception:
                continue
        plants.append({
            "id": plant_name,
            "display_name": localized.get(plant_name, plant_name),
            "seed_sprite": seed_sprite,
            "states": states,
            "kind": class_name or "plant",
            "item_id": harvest_id or item_name,
            "item_value": item_value,
            "harvest_count": harvest_count,
            "growth_seconds": 30.0,
            "classification": (
                "CONFIRMED plant_* prefab state_img sprites, seed ItemPickable "
                "icon, PlantWeapon/PlantMaterial fields; growth duration is a "
                "native reconstruction because PlantInfo/Resources data is "
                "not present in the extracted APK assets."),
        })

    plants.sort(key=lambda item: item["id"])
    document["plants"] = plants
    document["notes"] = (
        "Chest00/RGCoin prefab metadata/sprites plus the actual "
        "transfer_gate_02 portal sprite clip. Chest coin loot and lid "
        "animation are native test behavior; RGChest weapon selection and "
        "WeaponDropInfo are recovered in the 0.5.3 chest pipeline. Plant "
        "prefab state sprites and seed icons are recovered from the common "
        "bundle; growth duration is a marked native reconstruction.")
    args.meta.write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")
    print(f"Wrote {args.meta.relative_to(ROOT)}: {len(plants)} plants, "
          f"{len(sprites)} sprites, {len(textures)} textures")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
