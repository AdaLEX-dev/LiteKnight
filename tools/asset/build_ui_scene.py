#!/usr/bin/env python3
"""Extract local-only native UI atlases and sprite metadata.

This targets the recovered title and UI SpriteAtlas textures used by the main
menu, HUD and pause/settings screens. Pixels remain LOCAL_ONLY; only this tool
and derived manifests (without pixels) are committed.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BUNDLE_ROOT = ROOT / "analysis/apktool_out/assets/AssetBundles"
DEFAULT_OUTPUT = ROOT / "assets/generated/ui"

TEXTURE_PREFIXES = (
    "SpriteAtlasTexture-title",
    "SpriteAtlasTexture-ui",
    "SpriteAtlasTexture-share_ui",
)


def build(bundle_root: Path, output: Path) -> dict:
    import UnityPy

    files = [str(p) for p in bundle_root.rglob("*") if p.is_file()]
    # The main-menu background layers live in the built-in level1 scene
    # serialized files, not the AssetBundles UI atlas. Include their actual
    # sharedassets dependencies so the red swirl/title line can be recovered.
    data_root = ROOT / "analysis/apktool_out/assets/bin/Data"
    for name in ("level1", "sharedassets1.assets",
                 "sharedassets1.assets.split0", "sharedassets1.assets.split1",
                 "sharedassets1.assets.split2", "sharedassets1.resource"):
        path = data_root / name
        if path.is_file():
            files.append(str(path))
    env = UnityPy.load(*files)
    textures = {}
    sprites = []
    for obj in env.objects:
        if obj.type.name != "Texture2D":
            continue
        data = obj.read()
        name = str(getattr(data, "m_Name", ""))
        if not name.startswith(TEXTURE_PREFIXES):
            continue
        if name in textures:
            continue
        image = data.image.convert("RGBA")
        texture_id = str(obj.path_id)
        target = output / "textures" / f"{texture_id}.png"
        target.parent.mkdir(parents=True, exist_ok=True)
        image.save(target)
        textures[texture_id] = {
            "id": texture_id,
            "name": name,
            "png": str(target.relative_to(output)),
            "width": image.width,
            "height": image.height,
            "source_cab": getattr(obj.assets_file, "name", ""),
            "filter": "Point",
        }
    for obj in env.objects:
        if obj.type.name != "Sprite":
            continue
        try:
            data = obj.read()
        except Exception:
            continue
        rd = getattr(data, "m_RD", None)
        texture_ptr = getattr(rd, "texture", None) if rd else None
        texture_id = str(getattr(texture_ptr, "path_id", ""))
        if texture_id not in textures:
            continue
        rect = getattr(rd, "textureRect", None)
        if rect is None or float(rect.width) <= 0 or float(rect.height) <= 0:
            continue
        texture = textures[texture_id]
        x = float(rect.x)
        y = float(rect.y)
        w = float(rect.width)
        h = float(rect.height)
        # Unity rect origin is bottom-left; store native top-left UVs.
        uv = [x / texture["width"],
              (texture["height"] - y - h) / texture["height"],
              w / texture["width"], h / texture["height"]]
        if (uv[2] <= 0.0 or uv[3] <= 0.0 or uv[0] < -0.001 or
                uv[1] < -0.001 or uv[0] + uv[2] > 1.001 or
                uv[1] + uv[3] > 1.001):
            continue
        if any(existing["name"] == str(getattr(data, "m_Name", ""))
               for existing in sprites):
            continue
        sprites.append({
            "id": str(obj.path_id),
            "name": str(getattr(data, "m_Name", "")),
            "texture": texture_id,
            "rect_pixels": [x, y, w, h],
            "rect_uv": uv,
            "pivot": [float(getattr(data, "m_Pivot", None).x if getattr(data, "m_Pivot", None) else 0.5),
                      float(getattr(data, "m_Pivot", None).y if getattr(data, "m_Pivot", None) else 0.5)],
            "pixels_per_unit": float(getattr(data, "m_PixelsToUnits", 0.0)),
        })
    # Main-menu-only sprites recovered from level1: dark backdrop, red swirl,
    # and the thin title underline. These are actual reference images and are
    # exposed with stable names for the native menu painter.
    menu_specs = {
        61: "menu_backdrop",
        65: "menu_swirl",
        63: "menu_title_line",
    }
    for obj in env.objects:
        if obj.type.name != "Sprite" or obj.path_id not in menu_specs:
            continue
        if "sharedassets1" not in getattr(obj.assets_file, "name", ""):
            continue
        try:
            data = obj.read()
            rd = data.m_RD
            texture_ptr = rd.texture
            # Data-level paths can collide with AssetBundle path IDs (for
            # example path 3 exists in both). Keep a separate stable menu id.
            texture_id = f"menu_{texture_ptr.path_id}"
            if texture_id not in textures:
                texture = texture_ptr.read()
                image = texture.image.convert("RGBA")
                target = output / "textures" / f"{texture_id}.png"
                target.parent.mkdir(parents=True, exist_ok=True)
                image.save(target)
                textures[texture_id] = {
                    "id": texture_id,
                    "name": str(getattr(texture, "m_Name", texture_id)),
                    "png": str(target.relative_to(output)),
                    "width": image.width,
                    "height": image.height,
                    "source_cab": getattr(obj.assets_file, "name", ""),
                    "filter": "Point",
                }
            rect = rd.textureRect
            texture = textures[texture_id]
            x, y, w, h = float(rect.x), float(rect.y), float(rect.width), float(rect.height)
            sprites.append({
                "id": str(obj.path_id),
                "name": menu_specs[obj.path_id],
                "texture": texture_id,
                "rect_pixels": [x, y, w, h],
                "rect_uv": [x / texture["width"],
                            (texture["height"] - y - h) / texture["height"],
                            w / texture["width"], h / texture["height"]],
                "pivot": [float(data.m_Pivot.x), float(data.m_Pivot.y)],
                "pixels_per_unit": float(data.m_PixelsToUnits),
            })
        except Exception as error:
            print(f"menu sprite {obj.path_id} unavailable: {error}")
    sprites.sort(key=lambda item: (item["name"], item["id"]))
    manifest = {
        "schema": 2,
        "tool": "tools/asset/build_ui_scene.py",
        "note": "Reference-derived UI pixels; LOCAL_ONLY / NOT_FOR_REDISTRIBUTION.",
        "textures": sorted(textures.values(), key=lambda item: item["name"]),
        "sprites": sprites,
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle-root", type=Path, default=DEFAULT_BUNDLE_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    manifest = build(args.bundle_root, args.output)
    print(f"UI textures: {len(manifest['textures'])}")
    print(f"UI sprites: {len(manifest['sprites'])}")
    print(f"Manifest: {args.output / 'manifest.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
