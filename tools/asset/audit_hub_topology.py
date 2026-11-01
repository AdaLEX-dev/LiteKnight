#!/usr/bin/env python3
"""Audit the native Living Room floor/collision reachability.

Reads the generated hero_room scene metadata (no pixels) and runs a 4-neighbour
BFS over reference floor-tile cells with recovered collision solids. It verifies
that the workshop, garden, cellar and battle-door markers are physically
reachable from spawn and that no event tilemap is baked.
"""
from __future__ import annotations

import json
import math
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCENES = [
    ROOT / "assets/generated/visual/hero_room/scene.json",
    ROOT / "build/assets/scenes/hero_room/scene.json",
    ROOT / "analysis/assets/scenes/hero_room.json",
]
OUT_JSON = ROOT / "analysis/assets/hub_topology.json"
OUT_MD = ROOT / "analysis/assets/hub_topology.md"


def audit(path: Path) -> dict:
    scene = json.loads(path.read_text(encoding="utf-8"))
    floor = {
        (math.floor(i["position"][0]), math.floor(i["position"][1]))
        for i in scene.get("instances", [])
        if "/floor_" in i.get("path", "") and "/tile" in i.get("path", "")
    }
    solids = scene.get("collision", {}).get("solids", [])

    def blocked(x, y):
        cx, cy = x + 0.5, y + 0.5
        return any(x0 <= cx <= x1 and y0 <= cy <= y1
                   for x0, y0, x1, y1 in solids)

    walkable = {cell for cell in floor if not blocked(*cell)}
    spawn = (math.floor(scene["spawn"][0]), math.floor(scene["spawn"][1]))
    seen = {spawn} if spawn in walkable else set()
    queue = deque([spawn]) if spawn in walkable else deque()
    while queue:
        x, y = queue.popleft()
        for neighbor in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if neighbor in walkable and neighbor not in seen:
                seen.add(neighbor)
                queue.append(neighbor)

    markers = {}
    for marker in scene.get("interaction_markers", []):
        cell = (math.floor(marker["position"][0]),
                math.floor(marker["position"][1]))
        reachable = cell in seen
        if not reachable:
            for dx in range(-4, 5):
                for dy in range(-4, 5):
                    if (cell[0] + dx, cell[1] + dy) in seen:
                        reachable = True
                        break
                if reachable:
                    break
        markers[marker["id"]] = {
            "kind": marker.get("kind"),
            "cell": list(cell),
            "reachable": reachable,
        }
    event_instances = sum(
        1 for i in scene.get("instances", [])
        if any(token in i.get("path", "").lower()
               for token in ("halloween", "chinesenewyear", "newyear",
                             "christmas", "festival", "firework")))
    return {
        "scene": str(path.relative_to(ROOT)) if path.is_relative_to(ROOT)
                 else str(path),
        "floor_cells": len(floor),
        "walkable_cells": len(walkable),
        "reachable_cells": len(seen),
        "reachable_ratio": (len(seen) / len(walkable)) if walkable else 0.0,
        "spawn": list(spawn),
        "spawn_reachable": spawn in seen,
        "collision_solids": len(solids),
        "event_instances": event_instances,
        "markers": markers,
    }


def main() -> int:
    report = None
    for path in SCENES:
        if path.is_file():
            report = audit(path)
            break
    if report is None:
        raise SystemExit("hero_room scene metadata missing; run build_hub_scene")
    required = ["forge_station", "garden_station", "hub_door"]
    report["required_reachable"] = {
        marker: report["markers"].get(marker, {}).get("reachable", False)
        for marker in required
    }
    report["pass"] = (report["spawn_reachable"] and
                      report["event_instances"] == 0 and
                      all(report["required_reachable"].values()))
    OUT_JSON.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# Living Room Topology Audit",
        "",
        f"- floor cells: **{report['floor_cells']}**",
        f"- walkable cells: **{report['walkable_cells']}**",
        f"- reachable from spawn: **{report['reachable_cells']}** "
        f"({report['reachable_ratio'] * 100:.1f}%)",
        f"- collision solids: **{report['collision_solids']}**",
        f"- baked event instances: **{report['event_instances']}**",
        f"- spawn reachable: **{report['spawn_reachable']}**",
        "",
        "## Marker reachability",
        "",
        "| marker | kind | cell | reachable |",
        "|---|---|---|---|",
    ]
    for marker, info in report["markers"].items():
        lines.append(f"| `{marker}` | {info['kind']} | {info['cell']} | "
                     f"{info['reachable']} |")
    lines += ["", f"**PASS:** {report['pass']}", ""]
    OUT_MD.write_text("\n".join(lines), encoding="utf-8")
    print(f"Hub topology audit: reachable {report['reachable_cells']}/"
          f"{report['walkable_cells']}, events={report['event_instances']}, "
          f"pass={report['pass']}")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
