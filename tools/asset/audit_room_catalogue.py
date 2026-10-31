#!/usr/bin/env python3
"""Catalogue actual PatternRoom prefabs and which ones are integrated."""
from __future__ import annotations
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUNDLE = ROOT / "analysis/apktool_out/assets/AssetBundles/room"
OUT = ROOT / "analysis/assets/room_catalogue.json"
SCENES = ROOT / "analysis/assets/scenes"

def room_number(name: str) -> int:
    match = re.fullmatch(r"r1_(\d+)", name)
    return int(match.group(1)) if match else 0

def main() -> int:
    try:
        import UnityPy
    except ImportError as error:
        raise SystemExit(f"UnityPy required for room catalogue: {error}")
    if not BUNDLE.is_dir():
        raise SystemExit(f"room bundle directory missing: {BUNDLE}")
    bundle_files = [str(path) for path in sorted(BUNDLE.iterdir()) if path.is_file()]
    if not bundle_files:
        raise SystemExit(f"room bundle directory is empty: {BUNDLE}")
    env = UnityPy.load(*bundle_files)
    rooms = {}
    for obj in env.objects:
        if obj.type.name != "GameObject":
            continue
        try:
            data = obj.read()
        except Exception:
            continue
        name = getattr(data, "m_Name", "")
        if not re.fullmatch(r"r1_\d+", name):
            continue
        entry = rooms.setdefault(name, {"id": name, "number": room_number(name),
                                        "source_path": f"Assets/RGPrefab/Room/PatternRoom/{name}.prefab",
                                        "integrated": False})
        for component in getattr(data, "m_Component", []):
            if component.component.type.name != "MonoBehaviour":
                continue
            try:
                script = component.component.read()
            except Exception:
                continue
            if hasattr(script, "room_width"):
                entry["width"] = script.room_width
                entry["height"] = script.room_height
                entry["offset_x"] = script.room_x_offset
                entry["offset_y"] = script.room_y_offset
    for name, entry in rooms.items():
        scene = SCENES / f"{name}_visual.json"
        if scene.is_file():
            data = json.loads(scene.read_text(encoding="utf-8"))
            entry.update({
                "integrated": True,
                "scene_metadata": str(scene.relative_to(ROOT)),
                "instances": len(data.get("instances", [])),
                "sprites": len(data.get("sprites", [])),
                "doors": len(data.get("collision", {}).get("doors", [])),
                "obstacle_markers": len(data.get("obstacles", {}).get("markers", [])),
                "collision_solids": len(data.get("collision", {}).get("solids", [])),
            })
    document = {
        "schema": 1,
        "tool": "tools/asset/audit_room_catalogue.py",
        "source_bundle": "analysis/apktool_out/assets/AssetBundles/room",
        "hub": {"id": "hero_room", "integrated": (SCENES / "hero_room.json").is_file(),
                "scene_metadata": "analysis/assets/scenes/hero_room.json"},
        "rooms": [rooms[name] for name in sorted(rooms, key=room_number)],
        "counts": {
            "discovered_pattern_rooms": len(rooms),
            "integrated_pattern_rooms": sum(1 for e in rooms.values() if e["integrated"]),
        },
        "note": "All entries are actual r1_N PatternRoom GameObjects from the reference room bundle. Native runtime currently integrates r1_1, r1_2 and r1_3; map-level reference generation remains UNKNOWN.",
    }
    OUT.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Wrote {OUT.relative_to(ROOT)}: {len(rooms)} rooms, "
          f"{document['counts']['integrated_pattern_rooms']} integrated")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
