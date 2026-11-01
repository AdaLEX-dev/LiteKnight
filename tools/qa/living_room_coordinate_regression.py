#!/usr/bin/env python3
"""Strict Living Room coordinate regression.

Every target below is the coordinate supplied by the bug report. The report
prints the nearest recovered object, distance, expected state, actual state and
PASS/FAIL. Run from the project root:

    python3 tools/qa/living_room_coordinate_regression.py
"""
from __future__ import annotations

import json
import math
import sys
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCENE = ROOT / "analysis/assets/scenes/hero_room.json"
ITEMS = ROOT / "analysis/assets/items/item_assets.json"

PLAYER_HALF = 0.4
PLAYER_OFFSET = 0.5
failures: list[str] = []
checks = 0


def load() -> tuple[dict, dict]:
    scene = json.loads(SCENE.read_text(encoding="utf-8"))
    items = json.loads(ITEMS.read_text(encoding="utf-8"))
    return scene, items


def distance(a, b) -> float:
    return math.hypot(a[0] - b[0], a[1] - b[1])


def nearest(scene: dict, x: float, y: float, path_filter=None):
    best = None
    for instance in scene["instances"]:
        path = instance.get("path", "")
        if path_filter is not None and path_filter not in path:
            continue
        d = distance(instance["position"], (x, y))
        if best is None or d < best[0]:
            best = (d, instance)
    return best


def paths(scene: dict, token: str) -> list[dict]:
    return [i for i in scene["instances"] if token in i.get("path", "")]


def marker(scene: dict, marker_id: str):
    return next((m for m in scene.get("interaction_markers", [])
                 if m.get("id") == marker_id), None)


def markers_near(scene: dict, x: float, y: float, radius: float):
    return [(distance(m.get("position", [999, 999]), (x, y)), m)
            for m in scene.get("interaction_markers", [])
            if distance(m.get("position", [999, 999]), (x, y)) <= radius]


def solids_near(scene: dict, x: float, y: float, radius: float = 0.8):
    result = []
    for box in scene["collision"]["solids"]:
        dx = max(box[0] - x, 0.0, x - box[2])
        dy = max(box[1] - y, 0.0, y - box[3])
        d = math.hypot(dx, dy)
        if d <= radius:
            result.append((d, box))
    return sorted(result)


def box_contains(box, x: float, y: float) -> bool:
    return box[0] <= x <= box[2] and box[1] <= y <= box[3]


def move(scene: dict, foot, delta):
    """Axis-separated player movement using the recovered solids."""
    solids = scene["collision"]["solids"]
    center = [foot[0], foot[1] + PLAYER_OFFSET]
    start = [center[0], center[1]]
    target = [center[0] + delta[0], center[1]]
    blocked_x = blocked_y = False
    start_box = [start[0] - PLAYER_HALF, start[1] - PLAYER_HALF,
                 start[0] + PLAYER_HALF, start[1] + PLAYER_HALF]
    target_box = [target[0] - PLAYER_HALF, target[1] - PLAYER_HALF,
                  target[0] + PLAYER_HALF, target[1] + PLAYER_HALF]
    for box in solids:
        vertical = start_box[1] < box[3] and start_box[3] > box[1]
        if not vertical:
            continue
        if delta[0] > 0 and start_box[2] <= box[0] and target_box[2] > box[0]:
            target[0] = min(target[0], box[0] - PLAYER_HALF)
            blocked_x = True
        elif delta[0] < 0 and start_box[0] >= box[2] and target_box[0] < box[2]:
            target[0] = max(target[0], box[2] + PLAYER_HALF)
            blocked_x = True
    center[0] = target[0]
    start_box = [center[0] - PLAYER_HALF, start[1] - PLAYER_HALF,
                 center[0] + PLAYER_HALF, start[1] + PLAYER_HALF]
    target = [center[0], center[1] + delta[1]]
    target_box = [target[0] - PLAYER_HALF, target[1] - PLAYER_HALF,
                  target[0] + PLAYER_HALF, target[1] + PLAYER_HALF]
    for box in solids:
        horizontal = start_box[0] < box[2] and start_box[2] > box[0]
        if not horizontal:
            continue
        if delta[1] > 0 and start_box[3] <= box[1] and target_box[3] > box[1]:
            target[1] = min(target[1], box[1] - PLAYER_HALF)
            blocked_y = True
        elif delta[1] < 0 and start_box[1] >= box[3] and target_box[1] < box[3]:
            target[1] = max(target[1], box[3] + PLAYER_HALF)
            blocked_y = True
    center[1] = target[1]
    return (center[0], center[1] - PLAYER_OFFSET), blocked_x, blocked_y


def report(target: str, reference, expected: str, actual: str, ok: bool,
           nearest_text: str = "") -> None:
    global checks
    checks += 1
    status = "PASS" if ok else "FAIL"
    if not ok:
        failures.append(target)
    print("LIVING_ROOM_COORDINATE_REGRESSION")
    print(f"Target: {target}")
    print(f"Reference: ({reference[0]:.3f}, {reference[1]:.3f})")
    if nearest_text:
        print(f"Nearest: {nearest_text}")
    print(f"Expected: {expected}")
    print(f"Actual: {actual}")
    print(f"{status}\n")


def require_no_scene_path(scene, target, ref, token, label):
    found = paths(scene, token)
    nearest_row = nearest(scene, ref[0], ref[1])
    nearest_text = "none"
    if nearest_row:
        d, instance = nearest_row
        nearest_text = (f"{instance.get('path')} at "
                        f"({instance['position'][0]:.3f},"
                        f"{instance['position'][1]:.3f}) d={d:.3f}")
    report(target, ref, f"no {label} object", f"{len(found)} matching object(s)",
           not found, nearest_text)


def require_marker(scene, target, ref, marker_id, kind, label):
    m = marker(scene, marker_id)
    ok = m is not None and m.get("kind") == kind and \
        distance(m.get("position", [999, 999]), ref) <= 3.1
    actual = "missing" if m is None else \
        f"kind={m.get('kind')} pos={m.get('position')} radius={m.get('radius')}"
    report(target, ref, f"{label} marker kind={kind}", actual, ok)


def main() -> int:
    scene, items = load()
    scene_sprites = {s["id"]: s for s in scene["sprites"]}

    # 1. Sign removal at (-12.630, 6.080)
    ref = (-12.630, 6.080)
    found = paths(scene, "roomdecorateslot_room_drink_seller")
    near = solids_near(scene, *ref, 0.6)
    require_no_scene_path(scene, "sign_drink_seller", ref,
                          "roomdecorateslot_room_drink_seller",
                          "generic signboard")
    report("sign_collision_at_(-12.630,6.080)", ref, "no sign collider",
           f"{len(near)} nearby solid(s)", not near,
           f"nearest solid d={near[0][0]:.3f}" if near else "none")
    sign_markers = markers_near(scene, *ref, 2.0)
    report("sign_interaction_at_(-12.630,6.080)", ref,
           "no sign interaction/text marker", f"{len(sign_markers)} marker(s)",
           not sign_markers,
           ", ".join(m.get("id", "?") for _, m in sign_markers) or "none")

    # 2. Mission board restoration
    ref = (-7.065, 5.130)
    board = paths(scene, "room/objects/task/board")
    board_nearest = nearest(scene, ref[0], ref[1], "room/objects/task/board")
    m = marker(scene, "mission_board")
    board_ok = bool(board) and m is not None and \
        m.get("kind") == "mission" and len(m.get("talk_lines", [])) > 0
    nearest_text = "none"
    if board_nearest:
        d, inst = board_nearest
        nearest_text = (f"{inst['path']} at "
                        f"({inst['position'][0]:.3f},"
                        f"{inst['position'][1]:.3f}) d={d:.3f}")
    report("mission_board_(-7.065,5.130)", ref,
           "recovered task/board sprite + mission interaction",
           f"board_instances={len(board)} marker="
           f"{'yes' if m else 'no'} tasks="
           f"{len(m.get('talk_lines', [])) if m else 0}",
           board_ok, nearest_text)
    board_top = next((i for i in board
                      if i.get("path", "").endswith("/top/SpriteRenderer")),
                     None)
    board_bottom = next((i for i in board
                         if i.get("path", "").endswith("/bottom/SpriteRenderer")),
                        None)
    board_solid = solids_near(scene, -9.22, 7.18, 0.8)
    board_geometry_ok = board_top is not None and board_bottom is not None and \
        board_top.get("sprite") == "hub_spr_1493" and \
        board_bottom.get("sprite") == "hub_spr_1414" and \
        distance(board_top["position"], (-9.22, 7.18)) <= 0.2 and \
        bool(board_solid)
    report("mission_board_prefab_(-7.065,5.130)", ref,
           "recovered hub_spr_1493/hub_spr_1414 board at wall with solid",
           f"top={board_top.get('sprite') if board_top else 'missing'} "
           f"bottom={board_bottom.get('sprite') if board_bottom else 'missing'} "
           f"solids={len(board_solid)}", board_geometry_ok)

    # 3. Weapon chest
    ref = (-2.643, 5.410)
    static_chest = paths(scene, "room/objects/chest")
    chest_marker = marker(scene, "weapon_chest")
    chest_definition = next((c for c in items.get("chests", [])
                             if c.get("id") == "hub_weapon_chest"), None)
    chest_random = chest_definition is not None and \
        not chest_definition.get("appointed_weapon") and \
        chest_definition.get("weapon_drop_level", -1) != -1
    chest_ok = not static_chest and chest_marker is not None and \
        chest_marker.get("kind") == "weapon_chest" and chest_random
    report("weapon_chest_(-2.643,5.410)", ref,
           "real chest00 marker; no static duplicate",
           f"static={len(static_chest)} marker="
           f"{'yes' if chest_marker else 'no'} definition="
           f"{'yes' if chest_definition else 'no'} random={chest_random}",
           chest_ok,
           nearest(scene, ref[0], ref[1])[1]["path"] if nearest(scene, *ref) else "none")
    chest_visuals = [v.get("sprite") for v in
                     (chest_definition or {}).get("visuals", [])]
    chest_marker_ok = chest_marker is not None and \
        distance(chest_marker.get("position", [999, 999]), ref) <= 1.2
    report("weapon_chest_react_(-2.643,5.410)", ref,
           "chest marker within 1.2 units; recovered chest00 visual set",
           f"marker_distance="
           f"{distance(chest_marker['position'], ref) if chest_marker else 999:.2f} "
           f"visuals={','.join(chest_visuals)}",
           chest_marker_ok and "chest00_box06" in chest_visuals)

    # 4. Table characters
    ref = (-0.280, 0.940)
    seats = [
        ("c13", (0.58, 0.32), "characters/c13/img/body"),
        ("c04", (-2.445, -0.95), "characters/c04/img/body"),
        ("c08", (2.942, -0.902), "characters/c08/img/body"),
    ]
    table_details = []
    table_ok = True
    for character_id, chair_position, body_token in seats:
        body_row = nearest(scene, *chair_position, body_token)
        chair_row = nearest(scene, *chair_position,
                            "table_default/table/chair")
        if body_row is None or chair_row is None:
            table_ok = False
            table_details.append(f"{character_id}=missing")
            continue
        _, body = body_row
        _, chair_inst = chair_row
        bx, by = body["position"]
        cx, cy = chair_inst["position"]
        seat_dy = by - (cy + 0.40)
        aligned = abs(bx - cx) <= 0.12 and abs(seat_dy) <= 0.10
        depth_ok = body.get("sorting_layer", 0) > chair_inst.get(
            "sorting_layer", 0)
        table_ok = table_ok and aligned and depth_ok
        table_details.append(
            f"{character_id}=({bx:.2f},{by:.2f}) chair=({cx:.2f},{cy:.2f}) "
            f"seat_delta={seat_dy:+.2f} depth={body.get('sorting_layer')}>"
            f"{chair_inst.get('sorting_layer')}")
    report("table_characters_(-0.280,0.940)", ref,
           "c13/c04/c08 feet on chair seat plane, character in front of chair",
           "; ".join(table_details), table_ok)

    # 5. Remove three-segment stair wall
    ref = (-10.405, -9.080)
    stair = paths(scene, "hub/stair_wall_repair")
    block = [s for s in scene["collision"]["solids"]
             if abs(s[0] + 12) < 0.01 and abs(s[1] + 12) < 0.01]
    at_target = solids_near(scene, *ref, 0.2)
    # A floor-edge solid (<=0.02 thick) is the intended dark-void boundary;
    # only the old artificial 3x3 wall block is forbidden.
    thick = [(d, b) for d, b in at_target
             if (b[2] - b[0]) > 0.05 and (b[3] - b[1]) > 0.05]
    floor_cells = [(i["position"][0], i["position"][1])
                   for i in scene["instances"]
                   if i.get("path", "").startswith("room_tilemap/floor_")
                   and -12.0 <= i["position"][0] <= -9.0
                   and -12.0 <= i["position"][1] <= -9.0]
    ok = not stair and not block and not thick and not floor_cells
    report("remove_three_walls_(-10.405,-9.080)", ref,
           "no repair wall visual, 3x3 artificial solid or floor tile; "
           "area stays outside-world",
           f"visuals={len(stair)} block={len(block)} "
           f"thick_at_target={len(thick)} floor_tiles={len(floor_cells)}", ok)

    # 6. Portal frame and nearby NPC
    ref = (13.724, -14.920)
    portal = paths(scene, "room/objects/magic_protal1")
    portal_marker = marker(scene, "dungeon_portal")
    explorer = marker(scene, "npc_explorer")
    portal_ok = bool(portal) and portal_marker is not None and \
        portal_marker.get("kind") == "door" and explorer is not None and \
        explorer.get("kind") == "npc" and len(explorer.get("talk_lines", [])) > 0
    report("portal_frame_and_npc_(13.724,-14.920)", ref,
           "magic_protal1 visuals + portal interaction + talkable explorer",
           f"portal_visuals={len(portal)} portal_marker="
           f"{'yes' if portal_marker else 'no'} explorer="
           f"{'yes' if explorer else 'no'}", portal_ok)
    portal_top = next((i for i in portal
                       if i.get("path", "").endswith("/top/SpriteRenderer")),
                      None)
    portal_bottom = next((i for i in portal
                          if i.get("path", "").endswith("/bottom/SpriteRenderer")),
                         None)
    explorer_body = nearest(scene, *(explorer["position"] if explorer else
                                    (16.41, -17.37)),
                            "npc_explorer/img/body")
    portal_details_ok = portal_top is not None and portal_bottom is not None and \
        portal_top.get("sprite") == "hub_spr_2248" and \
        portal_bottom.get("sprite") == "hub_spr_2215" and \
        portal_top.get("sorting_layer") == 6 and \
        portal_bottom.get("sorting_layer") == 4 and \
        explorer is not None and len(explorer.get("talk_lines", [])) >= 2 and \
        explorer_body is not None and \
        distance(explorer["position"], (13.724, -14.920)) <= 4.2
    report("portal_frame_details_(13.724,-14.920)", ref,
           "hub_spr_2248 top + hub_spr_2215 base at recovered portal; "
           "talkable explorer within 4.2",
           f"top={portal_top.get('sprite') if portal_top else 'missing'} "
           f"bottom={portal_bottom.get('sprite') if portal_bottom else 'missing'} "
           f"explorer_body={'yes' if explorer_body else 'no'} "
           f"explorer_dist="
           f"{distance(explorer['position'], ref) if explorer else 999:.2f}",
           portal_details_ok)

    # 7. Sign removal at (21.034, -21.840)
    ref = (21.034, -21.840)
    require_no_scene_path(scene, "sign_mysterious_statue", ref,
                          "roomdecorateslot_room_mysteriou_statue",
                          "mysterious-statue signboard")
    near = solids_near(scene, *ref, 0.6)
    report("sign_collision_at_(21.034,-21.840)", ref, "no sign collider",
           f"{len(near)} nearby solid(s)", not near,
           f"nearest solid d={near[0][0]:.3f}" if near else "none")
    sign_markers = markers_near(scene, *ref, 2.0)
    report("sign_interaction_at_(21.034,-21.840)", ref,
           "no sign interaction/text marker", f"{len(sign_markers)} marker(s)",
           not sign_markers,
           ", ".join(m.get("id", "?") for _, m in sign_markers) or "none")

    # 8/9. Table and object footprints
    for target, ref, expected_sprite, expected_width in (
            ("table_collision_17", (17.400, -26.440), "hub_spr_2229", 8.0),
            ("object_collision_27", (27.600, -26.440), "hub_spr_2240", 8.0)):
        object_row = nearest(scene, *ref, "room_tilemap/object_portal/tile")
        near = solids_near(scene, *ref, 0.25)
        identity_ok = object_row is not None and \
            object_row[1].get("sprite") == expected_sprite
        ok = bool(near) and (near[0][1][2] - near[0][1][0]) >= expected_width
        if near:
            box = near[0][1]
            probes = [
                ((ref[0], box[3] + 1.4), (0.0, -4.0)),
                ((ref[0], box[1] - 1.4), (0.0, 4.0)),
                ((box[0] - 1.4, (box[1] + box[3]) * 0.5), (4.0, 0.0)),
                ((box[2] + 1.4, (box[1] + box[3]) * 0.5), (-4.0, 0.0)),
                ((box[0] - 1.4, box[3] + 1.4), (4.0, -4.0)),
                ((box[2] + 1.4, box[1] - 1.4), (-4.0, 4.0)),
            ]
            for start, delta in probes:
                out, bx, by = move(scene, start, delta)
                cx, cy = out[0], out[1] + PLAYER_OFFSET
                if box_contains(box, cx, cy):
                    ok = False
        report(target, ref,
               f"{expected_sprite} object; footprint >= {expected_width}; "
               "blocked from cardinal and diagonal approaches",
               f"object={object_row[1].get('sprite') if object_row else 'none'} "
               f"box={near[0][1] if near else 'none'}", ok and identity_ok,
               f"nearest={near[0][1]}" if near else "none")

    # 10. NPC + grave
    npc_ref = (31.345, -20.100)
    grave_ref = (32.515, -16.910)
    magic = marker(scene, "npc_magic")
    magic_dist = distance(magic["position"], grave_ref) if magic else 999.0
    magic_body = nearest(scene, 32.20, -17.94, "npc_magic/img/body")
    grave_visual = nearest(scene, *grave_ref, "npc_bossrush_mgr/SpriteRenderer")
    grave_dist = grave_visual[0] if grave_visual else 999.0
    grave_visual_ok = grave_visual is not None and grave_dist <= 3.0 and \
        grave_visual[1].get("sprite") == "hub_spr_1047"
    body_facing = bool(magic_body[1].get("flip_x")) if magic_body else False
    npc_ok = magic is not None and magic.get("kind") == "npc" and \
        len(magic.get("talk_lines", [])) >= 3 and magic_dist <= 1.5 and \
        magic_body is not None and body_facing and grave_visual_ok
    report("npc_grave_(31.345,-20.100)", npc_ref,
           "magic NPC moved along the NPC->grave axis, talkable, facing grave; "
           "recovered guardian/grave sprite within 3.0 of grave reference",
           f"marker_dist_to_reference={magic_dist:.2f} "
           f"body={'yes' if magic_body else 'no'} facing={body_facing} "
           f"grave_sprite={grave_visual[1].get('sprite') if grave_visual else 'none'} "
           f"grave_dist={grave_dist:.2f}", npc_ok)
    report("grave_reference_(32.515,-16.910)", grave_ref,
           "recovered grave/guardian visual present near reference",
           f"nearest={grave_visual[1].get('path') if grave_visual else 'none'} "
           f"d={grave_dist:.2f}", grave_visual_ok)

    # 11/12/13. Reforging tables
    forge_boxes = []
    for name, ref, table_sprite, max_top in (
            ("table_31", (31.012, -3.760), "hub_spr_2179", -3.9),
            ("table_35", (35.037, -3.131), "hub_spr_121", -4.0),
            ("table_39", (39.717, -3.326), "hub_spr_2195", -3.9)):
        near = solids_near(scene, *ref, 1.1)
        table_row = nearest(scene, *ref, "research/forge")
        ok = bool(near) and table_row is not None and \
            table_row[1].get("sprite") == table_sprite
        box = near[0][1] if near else None
        if box is not None:
            forge_boxes.append(box)
            # The top of the footprint must be at/below the reported table
            # approach line; the middle table allows one block more approach.
            if box[3] > max_top + 0.05:
                ok = False
            for start, delta in (
                    ((ref[0], box[3] + 1.4), (0.0, -4.0)),
                    ((ref[0], box[1] - 1.4), (0.0, 4.0)),
                    ((box[0] - 1.4, (box[1] + box[3]) * 0.5), (4.0, 0.0)),
                    ((box[2] + 1.4, (box[1] + box[3]) * 0.5), (-4.0, 0.0)),
                    ((box[0] - 1.4, box[3] + 1.4), (4.0, -4.0)),
                    ((box[2] + 1.4, box[1] - 1.4), (-4.0, 4.0))):
                out, bx, by = move(scene, start, delta)
                cx, cy = out[0], out[1] + PLAYER_OFFSET
                if box_contains(box, cx, cy):
                    ok = False
        report(name, ref,
               f"{table_sprite} table with per-table footprint; blocked from "
               "cardinal and diagonal approaches",
               f"table={table_row[1].get('sprite') if table_row else 'none'} "
               f"nearest={box}", ok)

    # 12b. Middle table approach proof.
    if forge_boxes:
        middle = min(forge_boxes, key=lambda b: abs((b[0] + b[2]) * 0.5 - 35.0))
        start = (35.037, -2.55)
        out, bx, by = move(scene, start, (0.0, -4.0))
        center_y = out[1] + PLAYER_OFFSET
        approach_ok = by and abs(center_y - (middle[3] + PLAYER_HALF)) < 0.08 \
            and center_y <= -3.5
        report("table_35_one_block_closer", (35.037, -3.131),
               "north approach stops near y=-3.63, not y=-2.63",
               f"stopped_center_y={center_y:.3f} top={middle[3]:.3f}",
               approach_ok)

    # 14. Rogue / beanbag
    ref = (10.454, -2.440)
    rogue = nearest(scene, 9.93, -1.64, "characters/c01/img/body")
    sofa = nearest(scene, 9.77, -1.0, "room/objects/lazy_sofa")
    rogue_ok = rogue is not None and sofa is not None
    rogue_detail = ""
    if rogue_ok:
        _, rogue_body = rogue
        _, sofa_inst = sofa
        sprites = {sprite["id"]: sprite for sprite in scene["sprites"]}
        sofa_sprite = sprites.get(sofa_inst.get("sprite"), {})
        rect = sofa_sprite.get("rect", [0, 0, 1, 1])
        ppu = float(sofa_sprite.get("pixels_per_unit", 16.0) or 16.0)
        pivot = sofa_sprite.get("pivot", [0.5, 0.5])
        height = float(rect[3]) / ppu * abs(sofa_inst.get("scale", [1, 1])[1])
        width = float(rect[2]) / ppu * abs(sofa_inst.get("scale", [1, 1])[0])
        sx, sy = sofa_inst["position"]
        bx, by = rogue_body["position"]
        within_sofa = abs(bx - sx) <= width * 0.6 and \
            sy - height * pivot[1] - 0.2 <= by <= sy + height * (1 - pivot[1])
        rogue_ok = within_sofa and rogue_body.get("sorting_layer") == 5 and \
            sofa_inst.get("sorting_layer") == 4 and \
            not sofa_inst.get("ground_depth", False) and \
            rogue_body.get("sprite") == "hub_spr_1439"
        rogue_detail = (f"body=({bx:.2f},{by:.2f}) sofa=({sx:.2f},{sy:.2f}) "
                        f"width={width:.2f} height={height:.2f} "
                        f"body_layer={rogue_body.get('sorting_layer')} "
                        f"sofa_layer={sofa_inst.get('sorting_layer')}")
    report("rogue_beanbag_(10.454,-2.440)", ref,
           "Rogue hub_spr_1439 body inside beanbag, drawn above it",
           f"body={'yes' if rogue else 'no'} sofa="
           f"{'yes' if sofa else 'no'} {rogue_detail}", rogue_ok)

    # 15. Sign removal at (13.790, 5.610)
    ref = (13.790, 5.610)
    require_no_scene_path(scene, "sign_safe", ref,
                          "room/objects/roomdecorateslot_safe",
                          "safe-placeholder signboard")
    near = solids_near(scene, *ref, 0.6)
    report("sign_collision_at_(13.790,5.610)", ref, "no sign collider",
           f"{len(near)} nearby solid(s)", not near,
           f"nearest solid d={near[0][0]:.3f}" if near else "none")
    sign_markers = markers_near(scene, *ref, 2.0)
    report("sign_interaction_at_(13.790,5.610)", ref,
           "no sign interaction/text marker", f"{len(sign_markers)} marker(s)",
           not sign_markers,
           ", ".join(m.get("id", "?") for _, m in sign_markers) or "none")

    # 16. Shorten wall
    ref = (6.600, -23.080)
    cap = [i for i in scene["instances"]
           if i.get("path", "").startswith("room_tilemap/wall")
           and abs(i["position"][0] - 7.5) < 0.01
           and abs(i["position"][1] + 22.5) < 0.01]
    near = solids_near(scene, *ref, 0.65)
    out, bx, by = move(scene, ref, (2.0, 0.0))
    back, back_bx, back_by = move(scene, (ref[0] + 2.0, ref[1]),
                                  (-2.0, 0.0))
    # The wall body immediately south must stay solid.
    south, south_bx, south_by = move(scene, ref, (0.0, -1.5))
    wall_ok = not cap and out[0] > ref[0] + 1.0 and back[0] < ref[0] + 1.2 \
        and south_by
    report("shorten_wall_(6.600,-23.080)", ref,
           "cap segment removed; walkway clear east-west; body wall south "
           "remains solid",
           f"cap_instances={len(cap)} moved_to_x={out[0]:.2f} "
           f"return_x={back[0]:.2f} south_blocked={south_by}",
           wall_ok)

    # 17. Invisible wall
    ref = (20.084, -10.920)
    near = solids_near(scene, *ref, 0.5)
    # The reported invisible wall must not exist at the target. The visible
    # wall one unit south must remain solid (it is a legitimate boundary).
    horizontal_ok = True
    for delta in ((2.0, 0.0), (-2.0, 0.0)):
        out, bx, by = move(scene, ref, delta)
        if bx or abs(out[0] - ref[0] - delta[0]) > 0.1:
            horizontal_ok = False
    north, north_bx, north_by = move(scene, ref, (0.0, 2.0))
    north_ok = not north_by and abs(north[1] - ref[1] - 2.0) <= 0.1
    south, south_bx, south_by = move(scene, ref, (0.0, -2.0))
    south_boundary_ok = south_by and south[1] > ref[1] - 1.4
    report("invisible_wall_(20.084,-10.920)", ref,
           "no collision at target; north/horizontal crossing clear; visible "
           "wall south remains solid",
           f"near_solids={len(near)} horizontal={horizontal_ok} "
           f"north={north_ok} south_wall={south_boundary_ok} "
           f"south_y={south[1]:.2f}",
           not near and horizontal_ok and north_ok and south_boundary_ok)

    # 18. Oxygen tanks
    ref = (23.625, -4.740)
    tank = nearest(scene, 23.45, -6.97, "research/bottle")
    tank_box = solids_near(scene, 23.45, -6.97, 0.4)
    tank_ok = tank is not None and bool(tank_box) and \
        tank[1].get("sprite") == "hub_spr_2172"
    detail = f"tank={'yes' if tank else 'no'} solids={len(tank_box)}"
    if tank_box:
        box = tank_box[0][1]
        detail += f" box=({box[0]:.2f},{box[1]:.2f},{box[2]:.2f},{box[3]:.2f})"
        # The report coordinate is near the upper body of the tall tanks;
        # the footprint must reach it, not just the base.
        tank_ok = tank_ok and box[3] >= ref[1] - 0.6
        for start, delta in (
                ((23.45, box[3] + 1.4), (0.0, -4.0)),
                ((23.45, box[1] - 1.4), (0.0, 4.0)),
                ((box[0] - 1.4, (box[1] + box[3]) * 0.5), (4.0, 0.0)),
                ((box[2] + 1.4, (box[1] + box[3]) * 0.5), (-4.0, 0.0)),
                ((box[0] - 1.4, box[3] + 1.4), (4.0, -4.0)),
                ((box[2] + 1.4, box[1] - 1.4), (-4.0, 4.0))):
            out, bx, by = move(scene, start, delta)
            if box_contains(box, out[0], out[1] + PLAYER_OFFSET):
                tank_ok = False
    report("oxygen_tanks_(23.625,-4.740)", ref,
           "recovered hub_spr_2172 bottle visual has footprint collision "
           "reaching the reported upper-body coordinate",
           detail, tank_ok)

    # 19. Key hubs
    for target, ref, marker_id, kind in (
            ("mech_boundary", (43.0, 6.9), "mech_station", "mech"),
            ("trader_station", (27.6, 4.4), "trader_station", "trader"),
            ("reforging_station", (35.92, -6.42), "forge_station", "forge"),
            ("material_chest", (48.93, 0.81), "material_chest",
             "material_chest"),
            ("weapon_station", (44.98, -6.3), "weapon_station",
             "weapon_station")):
        near = solids_near(scene, *ref, 1.0)
        m = marker(scene, marker_id)
        ok = m is not None and m.get("kind") == kind and bool(near)
        report(target, ref, f"{kind} marker + physical solid",
               f"marker={'yes' if m else 'no'} solids={len(near)}", ok)

    # 20. Garden entrance and autonomous Living Room walk.
    cells = set()
    for instance in scene["instances"]:
        if instance.get("path", "").startswith("room_tilemap/floor_"):
            cells.add((int(math.floor(instance["position"][0])),
                       int(math.floor(instance["position"][1]))))
    solids = scene["collision"]["solids"]

    def blocked_cell(x, y):
        cx, cy = x + 0.5, y + 0.5
        for box in solids:
            if cx + PLAYER_HALF > box[0] and cx - PLAYER_HALF < box[2] and \
               cy + PLAYER_HALF > box[1] and cy - PLAYER_HALF < box[3]:
                return True
        return False

    def find_free(cell):
        cell = (int(math.floor(cell[0])), int(math.floor(cell[1])))
        queue = deque([cell])
        seen = {cell}
        while queue:
            x, y = queue.popleft()
            if (x, y) in cells and not blocked_cell(x, y):
                return (x, y)
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (x + dx, y + dy)
                if n not in seen and -70 <= n[0] <= 60 and -35 <= n[1] <= 20:
                    seen.add(n)
                    queue.append(n)
        return None

    def bfs(start_cell, goal_cell, allowed=None):
        start = find_free(start_cell)
        goal = find_free(goal_cell)
        if start is None or goal is None:
            return None
        queue = deque([start])
        previous = {start: None}
        while queue:
            x, y = queue.popleft()
            if (x, y) == goal:
                path = []
                current = goal
                while current is not None:
                    path.append(current)
                    current = previous[current]
                return path[::-1]
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1),
                           (1, 1), (1, -1), (-1, 1), (-1, -1)):
                n = (x + dx, y + dy)
                if n in previous or n not in cells or blocked_cell(*n):
                    continue
                if allowed is not None and not allowed(*n):
                    continue
                if dx and dy and (blocked_cell(x + dx, y) or
                                  blocked_cell(x, y + dy)):
                    continue
                previous[n] = n
                previous[n] = (x, y)
                queue.append(n)
        return None

    lobby_to_garden = bfs((0, -9), (-30, 0))
    garden_to_lobby = bfs((-30, 0), (0, -9))
    # The tree boundary is solid outside the recovered opening. Restrict the
    # search to the y=6 row so a detour through another opening cannot mask a
    # hole in the boundary.
    tree_crossing = bfs((-16, 6), (-22, 6),
                       allowed=lambda x, y: y == 6)
    report("garden_entrance", (-30.0, 0.0),
           "walkable path lobby->garden and garden->lobby through intended "
           "opening",
           f"lobby_to_garden={lobby_to_garden is not None} "
           f"garden_to_lobby={garden_to_lobby is not None}", 
           lobby_to_garden is not None and garden_to_lobby is not None)
    report("garden_tree_boundary", (-19.0, 6.0),
           "tree boundary at y=6 remains solid",
           f"crossing={tree_crossing is not None}",
           tree_crossing is None)

    # Full autonomous walk from several player spawn/teleport positions to
    # every Living Room service. This is grid A* over the recovered collision
    # world, not a straight-line assumption.
    destinations = {
        "mission_board": (-7.065, 4.5),
        "weapon_chest": (-2.89, 5.3),
        "dungeon_portal": (13.53, -14.0),
        "trader": (27.6, 2.6),
        "reforging": (35.9, -2.0),
        "weapon_station": (44.98, -4.4),
        "material_chest": (48.93, 2.5),
        "mech": (43.0, 3.6),
        "garden": (-30.0, 0.0),
        "rogue_beanbag": (10.4, -1.4),
    }
    spawns = {
        "lobby": (0, -9),
        "portal": (13, -14),
        "workshop": (44, -6),
        "garden": (-28, 0),
        "beanbag": (10, -2),
    }
    for spawn_name, spawn_cell in spawns.items():
        failures_before = len(failures)
        unreachable = []
        for destination, destination_position in destinations.items():
            if bfs(spawn_cell, destination_position) is None:
                unreachable.append(destination)
        report(f"autonomous_walk_from_{spawn_name}", spawn_cell,
               "all Living Room destinations reachable without leaving the "
               "floor/solids",
               f"unreachable={','.join(unreachable) if unreachable else 'none'}",
               not unreachable)

    print(f"SUMMARY: {checks - len(failures)}/{checks} coordinate checks passed")
    if failures:
        print("FAILED TARGETS: " + ", ".join(failures))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
