#!/usr/bin/env python3
"""LiteKnight asset translation pipeline entry point.

Stages:
  extract/build -> scene metadata -> inventory -> audit -> native runtime

The existing UnityPy builder remains the extraction/translation implementation;
this CLI provides stable orchestration for developers and CI.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ASSET_TOOLS = ROOT / "tools/asset"
VENV_PYTHON = ASSET_TOOLS / ".venv/bin/python"


def run(command: list[str]) -> int:
    print("+", " ".join(str(part) for part in command), flush=True)
    return subprocess.call(command, cwd=ROOT)


def python_for_scene() -> str:
    return str(VENV_PYTHON) if VENV_PYTHON.is_file() else sys.executable


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=[
        "scene", "inventory", "audit", "all", "check-local"
    ])
    parser.add_argument("args", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    if args.command == "scene":
        return run([python_for_scene(), str(ASSET_TOOLS / "build_visual_scene.py"), *args.args])
    if args.command == "inventory":
        return run([sys.executable, str(ASSET_TOOLS / "build_asset_inventory.py"), *args.args])
    if args.command == "audit":
        return run([sys.executable, str(ASSET_TOOLS / "audit_assets.py"), *args.args])
    if args.command == "check-local":
        return run([sys.executable, str(ASSET_TOOLS / "audit_assets.py"),
                    "--require-generated", *args.args])
    result = run([python_for_scene(), str(ASSET_TOOLS / "build_visual_scene.py"), *args.args])
    if result != 0:
        return result
    result = run([sys.executable, str(ASSET_TOOLS / "build_asset_inventory.py")])
    if result != 0:
        return result
    return run([sys.executable, str(ASSET_TOOLS / "audit_assets.py")])


if __name__ == "__main__":
    raise SystemExit(main())
