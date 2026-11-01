#!/usr/bin/env python3
"""Copy the recovered Living Room seller visuals into the item pack.

The hub's `room_seller` prefab contains the actual seller NPC, shadow and
display-case sprites in the HeroRoom scene extraction. The dungeon seller room
(`r_sell`) has a reconstructed world placement but no serialized seller
visuals, so those exact recovered pixels are copied here for the runtime
seller. No new art is drawn.
"""
from __future__ import annotations

import hashlib
import json
import shutil
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
HERO = ROOT / "analysis/assets/scenes/hero_room.json"
HERO_TEXTURES = ROOT / "assets/generated/visual/hero_room"
META = ROOT / "analysis/assets/items/item_assets.json"
TEXTURE_OUT = ROOT / "assets/generated/items/textures"

COPY = {
    "hub_spr_962": ("seller_npc_body", "seller_npc_body"),
    "hub_spr_69": ("seller_npc_shadow", "seller_npc_shadow"),
    "hub_spr_1528": ("seller_pedestal", "seller_pedestal"),
    "hub_spr_2042": ("seller_counter", "seller_counter"),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    if not HERO.is_file() or not META.is_file():
        raise SystemExit("hero_room scene or item pack missing")
    hero = json.loads(HERO.read_text(encoding="utf-8"))
    hero_sprites = {sprite["id"]: sprite for sprite in hero["sprites"]}
    hero_textures = {texture["id"]: texture for texture in hero["textures"]}
    document = json.loads(META.read_text(encoding="utf-8"))
    sprites = document.setdefault("sprites", [])
    textures = document.setdefault("textures", [])
    sprite_ids = {sprite["id"] for sprite in sprites}
    texture_ids = {texture["id"] for texture in textures}
    TEXTURE_OUT.mkdir(parents=True, exist_ok=True)

    visuals = {}
    for source_id, (name, texture_name) in COPY.items():
        source = hero_sprites.get(source_id)
        if source is None:
            continue
        source_texture = hero_textures.get(source["texture"])
        if source_texture is None:
            continue
        png = HERO_TEXTURES / source_texture["png"]
        if not png.is_file():
            continue
        target_png = TEXTURE_OUT / f"{texture_name}.png"
        shutil.copyfile(png, target_png)
        texture_id = f"item_{texture_name}"
        if texture_id not in texture_ids:
            textures.append({
                "id": texture_id,
                "png": f"textures/{target_png.name}",
                "filter": "point",
                "wrap": "clamp",
            })
            texture_ids.add(texture_id)
        sprite_id = name
        if sprite_id not in sprite_ids:
            width, height = Image.open(png).size
            sprite = dict(source)
            sprite["id"] = sprite_id
            sprite["texture"] = texture_id
            sprite["texture_width"] = int(width)
            sprite["texture_height"] = int(height)
            sprites.append(sprite)
            sprite_ids.add(sprite_id)
        visuals[name] = sprite_id

    document["seller_visual"] = {
        "npc_body": visuals.get("seller_npc_body", ""),
        "npc_shadow": visuals.get("seller_npc_shadow", ""),
        "pedestal": visuals.get("seller_pedestal", ""),
        "counter": visuals.get("seller_counter", ""),
        "classification": (
            "CONFIRMED recovered room_seller npc01 body/shadow and display-case "
            "sprites copied from the HeroRoom extraction for the reconstructed "
            "r_sell seller placement."),
    }
    META.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")
    print(f"Wrote seller visuals into {META.relative_to(ROOT)}: {visuals}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
