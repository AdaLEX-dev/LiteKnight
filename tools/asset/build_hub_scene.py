#!/usr/bin/env python3
"""Build the native Living Room (HeroRoom) scene from the local reference level.

Source: assets/bin/Data/level2 + reconstructed sharedassets*.assets. The
Apktool output stores Unity shared assets as `.splitN` files; this tool joins
them in the local analysis tree (never committed) before loading level2.

All recovered pixels remain LOCAL_ONLY / NOT_FOR_REDISTRIBUTION. The committed
output is metadata only.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE_SCENE = ROOT / "analysis/assets/scenes/r1_1_visual.json"
DEFAULT_DATA = ROOT / "analysis/apktool_out/assets/bin/Data"
DEFAULT_OUTPUT = ROOT / "assets/generated/visual/hero_room"
DEFAULT_SCENE_METADATA = ROOT / "analysis/assets/scenes/hero_room.json"
DEFAULT_MANIFEST = ROOT / "analysis/assets/scenes/hero_room_manifest.json"


def join_split_assets(data_dir: Path) -> None:
    for base in ("sharedassets0.assets", "sharedassets1.assets", "sharedassets2.assets"):
        target = data_dir / base
        if target.exists():
            continue
        parts = sorted(data_dir.glob(base + ".split*"),
                       key=lambda p: int(re.search(r"split(\d+)$", p.name).group(1)))
        if parts:
            with target.open("wb") as out:
                for part in parts:
                    out.write(part.read_bytes())


def quat_z(rotation) -> float:
    if rotation is None:
        return 0.0
    return 2.0 * math.atan2(float(rotation.z), float(rotation.w))


def world_transform(pid, gos, tr_to_go):
    chain = []
    cur = pid
    seen = set()
    while cur in gos and cur not in seen:
        seen.add(cur)
        chain.append(cur)
        father = gos[cur]["father"]
        cur = tr_to_go.get(father, 0)
        if cur == 0:
            break
    pos = [0.0, 0.0]
    rot = 0.0
    scale = [1.0, 1.0]
    for node in reversed(chain):
        g = gos[node]
        local = g["local"]
        angle = rot
        c = math.cos(angle)
        s = math.sin(angle)
        px = local[0] * scale[0]
        py = local[1] * scale[1]
        pos[0] += px * c - py * s
        pos[1] += px * s + py * c
        rot += quat_z(g["rotation"])
        scale[0] *= g["scale"][0]
        scale[1] *= g["scale"][1]
    return pos, rot, scale


def build(data_dir: Path, output: Path, manifest_path: Path,
          metadata_path: Path) -> dict:
    join_split_assets(data_dir)
    import UnityPy

    files = [str(p) for p in data_dir.iterdir() if p.is_file() and ".split" not in p.name]
    env = UnityPy.load(*files)
    objects = [o for o in env.objects
               if getattr(o.assets_file, "name", "") == "level2"]

    gos = {}
    tr_to_go = {}
    for obj in objects:
        if obj.type.name != "GameObject":
            continue
        data = obj.read()
        tr = 0
        father = 0
        local = [0.0, 0.0, 0.0]
        rotation = None
        scale = [1.0, 1.0, 1.0]
        for comp in data.m_Component:
            if comp.component.type.name not in ("Transform", "RectTransform"):
                continue
            tr = comp.component.path_id
            try:
                transform = comp.component.read()
                father = getattr(getattr(transform, "m_Father", None), "m_PathID", 0) or 0
                local = [float(transform.m_LocalPosition.x),
                         float(transform.m_LocalPosition.y),
                         float(transform.m_LocalPosition.z)]
                rotation = getattr(transform, "m_LocalRotation", None)
                scale = [float(transform.m_LocalScale.x),
                         float(transform.m_LocalScale.y),
                         float(transform.m_LocalScale.z)]
            except Exception:
                pass
        gos[obj.path_id] = {
            "name": data.m_Name, "transform": tr, "father": father,
            "local": local, "rotation": rotation, "scale": scale,
            "active": bool(getattr(data, "m_IsActive", True)),
            "game_object": obj,
        }
        if tr:
            tr_to_go[tr] = obj.path_id

    def active_chain(pid):
        cur = pid
        seen = set()
        while cur in gos and cur not in seen:
            seen.add(cur)
            if not gos[cur].get("active", True):
                return False
            cur = tr_to_go.get(gos[cur]["father"], 0)
        return True

    event_tokens = ("halloween", "chinesenewyear", "newyear", "christmas",
                    "festival", "event_", "_event", "snow", "firework")

    def event_path(p):
        low = p.lower()
        return any(token in low for token in event_tokens)

    # Default Living Room cleanup requested by the 0.5.4 audit. These are
    # event/progression/tutorial objects that are active in the serialized
    # HeroRoom but not part of the ordinary room presentation.
    removed_prefixes = (
        "room/objects/cellar",          # staircase, cellar trigger, locks
        "room/objects/first_arrow",     # tutorial arrow
        "room/objects/npc_trainer",     # Drillmaster
        "room/objects/npc_sell_limit",  # limit seller opposite Drillmaster
        "room/objects/promotion",       # promotion NPC pair
        "room/objects/magic_ball",      # floating orb
        "room/objects/magic_protal1",   # lower portal 1
        "room/objects/magic_protal2",   # lower portal 2 / transfer gate
        "room/objects/npc_bossrush_mgr",# boss-rush sign/guardian
        "room_tilemap/floor_cellar",
        "room_tilemap/object_cellar",
        "room_tilemap_lights/portal",
    )

    def removed_path(p):
        low = p.lower()
        return (any(low.startswith(prefix) for prefix in removed_prefixes)
                or low.endswith("/tab"))  # biaoqing_16 (!) markers

    def path(pid):
        parts = []
        cur = pid
        seen = set()
        while cur in gos and cur not in seen:
            seen.add(cur)
            parts.append(gos[cur]["name"])
            cur = tr_to_go.get(gos[cur]["father"], 0)
            if cur == 0:
                break
        return "/".join(reversed(parts))

    # Start from the already-integrated r1_1 character/weapon metadata and
    # sprites so the hub player has the same animation/equipment path.
    base = json.loads(BASE_SCENE.read_text(encoding="utf-8"))
    textures = list(base.get("textures", []))
    sprites = list(base.get("sprites", []))
    animations = list(base.get("animations", []))
    characters = list(base.get("characters", []))
    weapons = list(base.get("weapons", []))
    player_stats = dict(base.get("player_stats", {}))
    texture_ids = {t["id"]: t for t in textures}
    sprite_ids = {s["id"]: s for s in sprites}

    # Base character/weapon textures are reused by the hub player/equipment.
    # Copy their already-generated local PNGs into the hub asset root.
    base_png_root = ROOT / "assets/generated/visual/r1_1_visual"
    for texture in textures:
        png = texture.get("png", "")
        if not png:
            continue
        source = base_png_root / png
        target = output / png
        if source.is_file() and source != target:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read_bytes())

    for animation in animations:
        source = ROOT / "analysis/assets/scenes" / animation
        target = output / animation
        if source.is_file() and not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read_bytes())

    used_textures = {}

    def ensure_texture(obj):
        data = obj.read()
        cab = getattr(getattr(obj, "assetsfile", None), "name", "")
        name = getattr(data, "m_Name", "")
        tag = hashlib.sha1(f"{cab}:{obj.path_id}:{name}".encode("utf-8")).hexdigest()[:12]
        tid = f"hub_tex_{tag}"
        if tid in texture_ids:
            return texture_ids[tid]
        image = data.image.convert("RGBA")
        target = output / "textures" / f"{tid}.png"
        target.parent.mkdir(parents=True, exist_ok=True)
        image.save(target)
        rgba = image.tobytes()
        entry = {
            "id": tid,
            "png": f"textures/{tid}.png",
            "source_bundle": "level2",
            "source_cab": cab,
            "source_path_id": str(obj.path_id),
            "source_name": name or tid,
            "width": image.width,
            "height": image.height,
            "format": "RGBA32",
            "filter": "point",
            "wrap": "clamp",
            "rgba8_sha256": hashlib.sha256(rgba).hexdigest(),
            "png_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
            "conversion": "Hub UnityPy Texture2D.image local conversion",
        }
        textures.append(entry)
        texture_ids[tid] = entry
        used_textures[tid] = entry
        return entry

    def ensure_sprite(sprite_ptr):
        if sprite_ptr is None or getattr(sprite_ptr, "path_id", 0) == 0:
            return None
        sprite = sprite_ptr.read()
        sid = f"hub_spr_{sprite_ptr.path_id}"
        if sid in sprite_ids:
            return sid
        rd = sprite.m_RD
        tex_ptr = rd.texture
        if tex_ptr is None or getattr(tex_ptr, "path_id", 0) == 0:
            return None
        texture = ensure_texture(tex_ptr)
        rect = rd.textureRect
        w = float(rect.width)
        h = float(rect.height)
        if w <= 0 or h <= 0:
            return None
        x = float(rect.x)
        y = float(rect.y)
        top = float(texture["height"]) - y - h
        pivot = sprite.m_Pivot
        entry = {
            "id": sid,
            "texture": texture["id"],
            "rect": [x, top, w, h],
            "pivot": [float(pivot.x), float(pivot.y)],
            "source_pivot": [float(pivot.x), float(pivot.y)],
            "pixels_per_unit": float(getattr(sprite, "m_PixelsToUnits", 0.0) or 1.0),
            "border": [0.0, 0.0, 0.0, 0.0],
            "filter": "point",
            "wrap": "clamp",
        }
        sprites.append(entry)
        sprite_ids[sid] = entry
        return sid

    instances = []
    for pid, g in gos.items():
        p = path(pid)
        if not p or p.startswith(("Canvas", "EventSystem", "gamemanager", "Main Camera")):
            continue
        if "/Canvas" in p:
            continue
        if "/masks/" in p or p.endswith("/masks"):
            # SpriteMask content needs a mask pass; skip rather than draw black.
            continue
        if p.startswith("room_tilemap_blackedge/"):
            # Black mask/curtain sprites would cover the floor with opaque
            # rectangles because the native renderer has no mask pass.
            continue
        if p == "room/objects/carpet":
            # The recovered carpet renderer is a near-opaque dark overlay whose
            # mask/material stack is not reconstructed; skip the void-like quad.
            continue
        if not active_chain(pid) or event_path(p) or removed_path(p):
            continue
        data = g["game_object"].read()
        pos, rot, scale = world_transform(pid, gos, tr_to_go)
        for comp in data.m_Component:
            if comp.component.type.name != "SpriteRenderer":
                continue
            try:
                sr = comp.component.read()
            except Exception:
                continue
            if not bool(getattr(sr, "m_Enabled", True)):
                continue
            sid = ensure_sprite(sr.m_Sprite)
            if sid is None:
                continue
            instances.append({
                "sprite": sid,
                "position": [pos[0], pos[1]],
                "rotation": rot,
                "scale": [scale[0] * float(sr.m_FlipX and -1 or 1), scale[1]],
                "color": [int(sr.m_Color.r * 255), int(sr.m_Color.g * 255),
                          int(sr.m_Color.b * 255), int(sr.m_Color.a * 255)],
                "sorting_layer": int(sr.m_SortingLayer),
                "sorting_layer_name": str(int(sr.m_SortingLayer)),
                "sorting_order": int(sr.m_SortingOrder),
                "flip_x": bool(sr.m_FlipX),
                "flip_y": bool(sr.m_FlipY),
                "path": p + "/SpriteRenderer",
            })
            break

    # Tilemaps: convert every tile to a sprite instance. This is the actual
    # recovered floor/wall/garden/light tile layout, not a painted mockup.
    tile_count = 0
    floor_tile_sprite = None
    wall_cell_solids = []
    for obj in objects:
        if obj.type.name != "Tilemap":
            continue
        go_id = None
        for other in objects:
            if other.type.name != "GameObject":
                continue
            d = other.read()
            for comp in d.m_Component:
                if comp.component.type.name == "Tilemap" and comp.component.path_id == obj.path_id:
                    go_id = other.path_id
                    break
            if go_id:
                break
        if go_id is None or go_id not in gos:
            continue
        p = path(go_id)
        if p.startswith("room_tilemap_blackedge"):
            # Black-edge/mask tilemap stack; see SpriteRenderer skip above.
            continue
        if not active_chain(go_id) or event_path(p) or removed_path(p):
            # Event tilemaps (for example chinesenewyear_wall_lobby) are
            # inactive in the default HeroRoom scene and must not be baked.
            continue
        pos, rot, scale = world_transform(go_id, gos, tr_to_go)
        tilemap = obj.read()
        # TilemapRenderer on the same GameObject (or child) for sorting.
        layer = 0
        order = 0
        data = gos[go_id]["game_object"].read()
        for comp in data.m_Component:
            if comp.component.type.name == "TilemapRenderer":
                try:
                    tr = comp.component.read()
                    layer = int(tr.m_SortingLayer)
                    order = int(tr.m_SortingOrder)
                except Exception:
                    pass
        for tile_pos, tile in tilemap.m_Tiles:
            index = int(getattr(tile, "m_TileSpriteIndex", -1))
            if index < 0 or index >= len(tilemap.m_TileSpriteArray):
                continue
            ref = tilemap.m_TileSpriteArray[index]
            sid = ensure_sprite(ref.m_Data)
            if sid is None:
                continue
            if floor_tile_sprite is None and layer in (2, 3) and "/floor_" in p:
                floor_tile_sprite = sid
            bx = float(tile_pos.x) + 0.5
            by = float(tile_pos.y) + 0.5
            tile_x = pos[0] + bx * scale[0]
            tile_y = pos[1] + by * scale[1]
            instances.append({
                "sprite": sid,
                "position": [tile_x, tile_y],
                "rotation": rot,
                "scale": [scale[0], scale[1]],
                "color": [255, 255, 255, 255],
                "sorting_layer": layer,
                "sorting_layer_name": str(layer),
                "sorting_order": order,
                "flip_x": False,
                "flip_y": False,
                "path": p + "/tile",
            })
            # Wall tilemaps are the actual physical wall layout. Convert each
            # wall cell into a solid so openings in the reference tile map are
            # traversable without the giant polygon-AABB blockers that made the
            # garden/workshop/cellar unreachable.
            if "/wall" in p or "gate_lobby" in p:
                half_x = abs(float(scale[0])) * 0.5
                half_y = abs(float(scale[1])) * 0.5
                wall_cell_solids.append([tile_x - half_x, tile_y - half_y,
                                         tile_x + half_x, tile_y + half_y])
            tile_count += 1

    colliders = list(wall_cell_solids)
    for pid, g in gos.items():
        p = path(pid)
        if not p or p.startswith(("Canvas", "EventSystem", "gamemanager", "Main Camera")):
            continue
        if "/Canvas" in p or "/masks/" in p:
            continue
        if not active_chain(pid) or event_path(p) or removed_path(p):
            continue
        # Progression locks and portal triggers are intentionally open in the
        # explorable hub; their visual objects remain rendered.
        if any(token in p for token in (
                "cellar_lock_obstacle", "cellar_obstacle",
                "magic_protal", "transfer_gate")):
            continue
        pos, rot, scale = world_transform(pid, gos, tr_to_go)
        data = g["game_object"].read()
        for comp in data.m_Component:
            t = comp.component.type.name
            if t not in ("BoxCollider2D", "CircleCollider2D",
                         "PolygonCollider2D", "CompositeCollider2D"):
                continue
            if t in ("PolygonCollider2D", "CompositeCollider2D"):
                # Composite wall polygons are often huge L-shaped outlines.
                # Their AABB would seal the whole room; wall tile cells now
                # provide the physical boundary. Small polygon furniture is
                # still covered by its sibling BoxCollider2D.
                continue
            try:
                col = comp.component.read()
            except Exception:
                continue
            if not bool(getattr(col, "m_Enabled", True)):
                continue
            if bool(getattr(col, "m_IsTrigger", False)):
                continue
            if t == "BoxCollider2D":
                sx = float(col.m_Size.x) * abs(scale[0])
                sy = float(col.m_Size.y) * abs(scale[1])
                ox = float(col.m_Offset.x) * scale[0]
                oy = float(col.m_Offset.y) * scale[1]
                minx, maxx = pos[0] + ox - sx * 0.5, pos[0] + ox + sx * 0.5
                miny, maxy = pos[1] + oy - sy * 0.5, pos[1] + oy + sy * 0.5
                if maxx - minx > 0.05 and maxy - miny > 0.05:
                    colliders.append([minx, miny, maxx, maxy])
            elif t == "CircleCollider2D":
                r = float(col.m_Radius) * max(abs(scale[0]), abs(scale[1]))
                ox = float(col.m_Offset.x) * scale[0]
                oy = float(col.m_Offset.y) * scale[1]
                if r > 0.05:
                    colliders.append([pos[0] + ox - r, pos[1] + oy - r,
                                      pos[0] + ox + r, pos[1] + oy + r])
            elif t == "PolygonCollider2D":
                polygon = getattr(col, "m_Points", None)
                paths = list(getattr(polygon, "m_Paths", []) or []) if polygon else []
                pts = [pt for path_points in paths for pt in path_points]
                if pts:
                    xs = [pos[0] + float(pt.x) * scale[0] for pt in pts]
                    ys = [pos[1] + float(pt.y) * scale[1] for pt in pts]
                    if max(xs) - min(xs) > 0.05 and max(ys) - min(ys) > 0.05:
                        colliders.append([min(xs), min(ys), max(xs), max(ys)])

    # Spawn in the lower living-room floor, clear of the central table.
    spawn = [0.0, -10.0]

    # Add the scene player render binding instances using the already-integrated
    # c00 body/shadow sprites from the r1_1 character set.
    c00 = next((c for c in characters if c.get("id") == "c00"), None)
    base_shadow = next((i for i in base.get("instances", [])
                        if i.get("path") == "c00/shadow"), None)
    if c00:
        body_sprite = c00.get("body_sprite")
        shadow_sprite = (base_shadow or {}).get("sprite")
        if body_sprite:
            instances.append({
                "sprite": body_sprite, "position": spawn, "rotation": 0.0,
                "scale": [1.0, 1.0], "color": [255, 255, 255, 255],
                "sorting_layer": 5, "sorting_layer_name": "Character",
                "sorting_order": 5, "flip_x": False, "flip_y": False,
                "path": "c00/img/body",
            })
        if shadow_sprite:
            instances.append({
                "sprite": shadow_sprite, "position": [spawn[0], spawn[1] - 0.05],
                "rotation": 0.0, "scale": [1.0, 1.0],
                "color": [255, 255, 255, 255],
                "sorting_layer": 3, "sorting_layer_name": "Shadow",
                "sorting_order": 1, "flip_x": False, "flip_y": False,
                "path": "c00/shadow",
            })

    # Collision bounds from the tile layout.
    if instances:
        xs = [i["position"][0] for i in instances]
        ys = [i["position"][1] for i in instances]
        minx, maxx = min(xs) - 2.0, max(xs) + 2.0
        miny, maxy = min(ys) - 2.0, max(ys) + 2.0
    else:
        minx, maxx, miny, maxy = -60.0, 60.0, -40.0, 40.0

    # Fill only cells that have no recovered floor tile with the recovered
    # cellar floor sprite. This closes unrecoverable mask/atlas holes without
    # painting over recovered tiles.
    if floor_tile_sprite is not None:
        occupied = set()
        for instance in instances:
            if instance.get("sorting_layer") in (2, 3) and "/tile" in instance.get("path", ""):
                occupied.add((math.floor(instance["position"][0]),
                              math.floor(instance["position"][1])))
        added = 0
        for cell_x in range(math.floor(minx), math.ceil(maxx)):
            for cell_y in range(math.floor(miny), math.ceil(maxy)):
                if (cell_x, cell_y) in occupied:
                    continue
                if added >= 5000:
                    break
                instances.append({
                    "sprite": floor_tile_sprite,
                    "position": [cell_x + 0.5, cell_y + 0.5],
                    "rotation": 0.0, "scale": [1.0, 1.0],
                    "color": [255, 255, 255, 255],
                    "sorting_layer": 2, "sorting_layer_name": "Floor",
                    "sorting_order": -50, "flip_x": False, "flip_y": False,
                    "path": "hub/floor_tile_fill",
                })
                added += 1
            if added >= 5000:
                break
        print("floor tile fill instances:", added)

    # A solid recovered-floor-colored fill behind the tilemaps closes gaps left
    # by reference tiles whose atlases are not fully recoverable. It is a native
    # fallback, not a substitute for the recovered tile layout.
    try:
        from PIL import Image as _Image
        fill_tex_id = "hub_fill_texture"
        fill_png = output / "textures" / "hub_fill.png"
        fill_png.parent.mkdir(parents=True, exist_ok=True)
        _Image.new("RGBA", (1, 1), (67, 52, 43, 255)).save(fill_png)
        textures.append({
            "id": fill_tex_id, "png": "textures/hub_fill.png",
            "source_bundle": "native", "source_cab": "native",
            "source_path_id": "hub_fill", "source_name": "Living Room floor fill",
            "width": 1, "height": 1, "format": "RGBA32",
            "filter": "point", "wrap": "clamp",
            "rgba8_sha256": hashlib.sha256(bytes((67, 52, 43, 255))).hexdigest(),
            "png_sha256": hashlib.sha256(fill_png.read_bytes()).hexdigest(),
            "conversion": "native floor fill",
        })
        fill_sprite = "hub_fill_sprite"
        sprites.append({
            "id": fill_sprite, "texture": fill_tex_id,
            "rect": [0.0, 0.0, 1.0, 1.0], "pivot": [0.5, 0.5],
            "source_pivot": [0.5, 0.5], "pixels_per_unit": 1.0,
            "border": [0.0, 0.0, 0.0, 0.0], "filter": "point", "wrap": "clamp",
        })
        instances.append({
            "sprite": fill_sprite,
            "position": [(minx + maxx) * 0.5, (miny + maxy) * 0.5],
            "rotation": 0.0,
            "scale": [maxx - minx, maxy - miny],
            "color": [255, 255, 255, 255],
            "sorting_layer": 2, "sorting_layer_name": "Floor",
            "sorting_order": -100, "flip_x": False, "flip_y": False,
            "path": "hub/floor_fill",
        })
    except Exception as exc:
        print("floor fill skipped:", exc)

    markers = [
        {"id": "character_station", "kind": "character", "label": "CHARACTER STATION",
         "position": [7.0, 5.0], "radius": 3.0},
        {"id": "forge_station", "kind": "forge", "label": "WEAPON WORKSHOP",
         "position": [44.98, -6.3], "radius": 4.0},
        {"id": "garden_station", "kind": "garden", "label": "PLANTER GARDEN",
         "position": [-26.0, 0.0], "radius": 4.0},
        {"id": "trader_station", "kind": "trader", "label": "ORB WEAPON TRADER",
         "position": [27.6, 3.7], "radius": 4.0},
        {"id": "hub_door", "kind": "door", "label": "DUNGEON",
         "position": [0.0, 7.5], "radius": 3.0},
    ]

    scene = {
        "id": "hero_room",
        "room": {
            "type": -1,
            "type_name": "hub",
            "stage": 0,
            "number": 0,
            "label": "LIVING ROOM",
            "clear_label": "",
            "classification": "CONFIRMED HeroRoom scene; hub label is a native presentation name",
        },
        "spawn": spawn,
        "camera": {"position": spawn, "orthographic_size": 10.0},
        "textures": textures,
        "sprites": sprites,
        "instances": instances,
        "animations": animations,
        "characters": characters,
        "player_stats": player_stats,
        "weapons": weapons,
        "enemy_definitions": [],
        "enemy_fixtures": [],
        "doors": [],
        "collision": {
            "bounds": [minx, miny, maxx - minx, maxy - miny],
            "solids": [[x0, y0, x1, y1]
                       for x0, y0, x1, y1 in colliders],
            "doors": [],
        },
        "interaction_markers": markers,
        "hub": True,
        "source": {
            "scene": "Assets/_RGScene/HeroRoom.unity",
            "level_file": "assets/bin/Data/level2",
            "classification": (
                "CONFIRMED HeroRoom tilemaps/SpriteRenderers/default active "
                "state and wall-tile collision; spawn, interaction markers "
                "and progression-lock removal are native development choices"
            ),
        },
    }
    output.mkdir(parents=True, exist_ok=True)
    scene_text = json.dumps(scene, indent=2, sort_keys=True) + "\n"
    (output / "scene.json").write_text(scene_text, encoding="utf-8")
    metadata_path.parent.mkdir(parents=True, exist_ok=True)
    metadata_path.write_text(scene_text, encoding="utf-8")
    manifest = {
        "schema": 1,
        "scene": str(output / "scene.json"),
        "textures": len(textures),
        "sprites": len(sprites),
        "instances": len(instances),
        "tile_instances": tile_count,
        "collision_solids": len(colliders),
        "classification": scene["source"]["classification"],
    }
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-dir", type=Path, default=DEFAULT_DATA)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--metadata", type=Path, default=DEFAULT_SCENE_METADATA)
    args = parser.parse_args()
    manifest = build(args.data_dir, args.output, args.manifest, args.metadata)
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
