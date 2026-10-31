#!/usr/bin/env python3
"""Build the local pixel-font atlas from the recovered Unity font asset.

The reference font is a dynamic TTF (`pixel_bold`) embedded in the local
AssetBundle tree. The TTF and rasterized atlas are reference-derived pixels and
remain LOCAL_ONLY; only this translator is committed.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BUNDLE_ROOT = ROOT / "analysis/apktool_out/assets/AssetBundles"
DEFAULT_OUTPUT = ROOT / "assets/generated/ui/font"

GLYPHS = [chr(code) for code in range(32, 127)]
FONT_NAME = "pixel_bold"
FONT_SIZE = 28
CELL_PADDING = 2
ALPHA_THRESHOLD = 96


def find_font(bundle_root: Path):
    import UnityPy

    files = [str(p) for p in bundle_root.rglob("*") if p.is_file()]
    env = UnityPy.load(*files)
    for obj in env.objects:
        if obj.type.name != "Font":
            continue
        data = obj.read()
        if getattr(data, "m_Name", "") == FONT_NAME:
            font_data = bytes(getattr(data, "m_FontData", b"") or b"")
            if not font_data:
                raise SystemExit(f"{FONT_NAME} has no embedded TTF data")
            return data, font_data
    raise SystemExit(f"reference font not found: {FONT_NAME}")


def build(bundle_root: Path, output: Path) -> dict:
    from PIL import Image, ImageDraw, ImageFont

    source, font_data = find_font(bundle_root)
    output.mkdir(parents=True, exist_ok=True)
    ttf_path = output / f"{FONT_NAME}.ttf"
    ttf_path.write_bytes(font_data)
    font = ImageFont.truetype(str(ttf_path), FONT_SIZE)

    ascent, descent = font.getmetrics()
    source_size = float(getattr(source, "m_FontSize", FONT_SIZE) or FONT_SIZE)
    scaled_spacing = float(getattr(source, "m_LineSpacing", ascent + descent)) * \
        (float(FONT_SIZE) / source_size)
    line_height = max(1, round(scaled_spacing), ascent + descent + 2)
    glyph_records = []
    max_width = 0
    for char in GLYPHS:
        bbox = font.getbbox(char, anchor="ls")
        advance = max(0, round(font.getlength(char)))
        if bbox is None:
            bbox = (0, 0, 0, 0)
        left, top, right, bottom = bbox
        width = max(0, right - left)
        height = max(0, bottom - top)
        glyph_records.append({
            "char": char,
            "codepoint": ord(char),
            "width": width,
            "height": height,
            "bearing_x": left,
            "bearing_y": top,
            "advance": advance,
        })
        max_width = max(max_width, width)

    columns = 16
    cell_width = max_width + CELL_PADDING * 2
    cell_height = max((g["height"] for g in glyph_records), default=1) + CELL_PADDING * 2
    rows = (len(glyph_records) + columns - 1) // columns
    atlas = Image.new("RGBA", (columns * cell_width, rows * cell_height), (255, 255, 255, 0))
    draw = ImageDraw.Draw(atlas)
    for index, glyph in enumerate(glyph_records):
        col = index % columns
        row = index // columns
        cell_x = col * cell_width
        cell_y = row * cell_height
        origin_x = cell_x + CELL_PADDING - glyph["bearing_x"]
        baseline_y = cell_y + CELL_PADDING - glyph["bearing_y"]
        draw.text((origin_x, baseline_y), glyph["char"], font=font,
                  fill=(255, 255, 255, 255), anchor="ls")
        glyph["x"] = cell_x + CELL_PADDING
        glyph["y"] = cell_y + CELL_PADDING
        glyph["atlas_width"] = cell_width
        glyph["atlas_height"] = cell_height

    # Force a crisp 1-bit alpha mask. The pixel font's native design is binary;
    # keeping subpixel antialias would blur it when scaled to UI pixels.
    pixels = atlas.load()
    for y in range(atlas.height):
        for x in range(atlas.width):
            r, g, b, a = pixels[x, y]
            pixels[x, y] = (255, 255, 255, 255 if a >= ALPHA_THRESHOLD else 0)

    # Reserve a 2x2 opaque white block for panels/borders. It is intentionally
    # outside the glyph grid so no glyph UV can bleed into it.
    white_x = 0
    white_y = atlas.height
    atlas = atlas.crop((0, 0, atlas.width, atlas.height + 4))
    white_layer = Image.new("RGBA", (4, 4), (255, 255, 255, 255))
    atlas.alpha_composite(white_layer, (white_x, white_y))

    png_path = output / f"{FONT_NAME}.png"
    atlas.save(png_path)
    document = {
        "schema": 1,
        "font": FONT_NAME,
        "source": "local reference Unity Font asset; LOCAL_ONLY / NOT_FOR_REDISTRIBUTION",
        "size": FONT_SIZE,
        "ascent": ascent,
        "descent": descent,
        "line_height": line_height,
        "texture": png_path.name,
        "atlas_width": atlas.width,
        "atlas_height": atlas.height,
        "cell_width": cell_width,
        "cell_height": cell_height,
        "alpha_threshold": ALPHA_THRESHOLD,
        "white_rect": [white_x, white_y, 4, 4],
        "glyphs": glyph_records,
    }
    json_path = output / f"{FONT_NAME}.json"
    json_path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return document


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle-root", type=Path, default=DEFAULT_BUNDLE_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    document = build(args.bundle_root, args.output)
    print(f"Font atlas: {args.output / (FONT_NAME + '.png')}")
    print(f"Metrics:    {args.output / (FONT_NAME + '.json')}")
    print(f"Glyphs: {len(document['glyphs'])}  atlas: {document['atlas_width']}x{document['atlas_height']}  line: {document['line_height']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
