#!/usr/bin/env python3
"""Build every recovered PatternRoom bundle (r1_1..r1_108).

The per-room scenes carry the actual room prefab hierarchy, wall/door topology,
obstacle markers, sprites and animation metadata. Generated output is local-only;
the runtime discovers every `r1_*_visual` directory present in the asset tree.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILDER = ROOT / "tools/asset/build_visual_scene.py"
PYTHON = ROOT / "tools/asset/.venv/bin/python"
SCENE_ROOT = ROOT / "analysis/assets/scenes/pattern"
WALL_ROOT = ROOT / "analysis/re/pattern"
OUTPUT_ROOT = ROOT / "assets/generated/visual"
BUNDLE_ROOT = ROOT / "analysis/apktool_out/assets/AssetBundles/room"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--first", type=int, default=1)
    parser.add_argument("--last", type=int, default=108)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    if not PYTHON.is_file():
        raise SystemExit(f"UnityPy venv missing: {PYTHON}")
    if not BUNDLE_ROOT.is_dir():
        raise SystemExit(f"room bundle root missing: {BUNDLE_ROOT}")
    SCENE_ROOT.mkdir(parents=True, exist_ok=True)
    WALL_ROOT.mkdir(parents=True, exist_ok=True)
    built = 0
    skipped = 0
    for index in range(args.first, args.last + 1):
        room = f"r1_{index}"
        source = OUTPUT_ROOT / f"{room}_visual"
        if source.joinpath("scene.json").is_file() and not args.force:
            skipped += 1
            continue
        command = [
            str(PYTHON), str(BUILDER),
            "--room", room,
            "--out", str(source),
            "--scene", str(SCENE_ROOT / f"{room}_visual.json"),
            "--manifest", str(SCENE_ROOT / f"{room}_visual_manifest.json"),
            "--wall-map", str(WALL_ROOT / f"{room}_wall_map.json"),
            "--spawn", "20", "15",
        ]
        result = subprocess.run(command, cwd=ROOT, check=False)
        if result.returncode != 0:
            print(f"{room}: FAILED", file=sys.stderr)
            return result.returncode
        built += 1
        print(f"{room}: ok", flush=True)
    print(f"Pattern rooms built: {built}, already present: {skipped}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
