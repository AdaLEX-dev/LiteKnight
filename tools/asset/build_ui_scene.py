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
        sprites.append({
            "id": str(obj.path_id),
            "name": str(getattr(data, "m_Name", "")),
            "texture": texture_id,
            "rect_pixels": [x, y, w, h],
            "rect_uv": [
                x / texture["width"],
                (texture["height"] - y - h) / texture["height"],
                w / texture["width"],
                h / texture["height"],
            ],
            "pivot": [float(getattr(data, "m_Pivot", None).x if getattr(data, "m_Pivot", None) else 0.5),
                      float(getattr(data, "m_Pivot", None).y if getattr(data, "m_Pivot", None) else 0.5)],
            "pixels_per_unit": float(getattr(data, "m_PixelsToUnits", 0.0)),
        })
    sprites.sort(key=lambda item: (item["name"], item["id"]))
    manifest = {
        "schema": 1,
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
