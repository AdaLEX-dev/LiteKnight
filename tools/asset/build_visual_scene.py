#!/usr/bin/env python3
"""Build the native r1_1 visual scene from real Unity objects.

This tool deliberately reuses the existing UnityPy conversion manifest. It does
not write a second texture decoder or commit proprietary pixels.

Outputs:
  assets/generated/visual/r1_1_visual/scene.json       (gitignored runtime scene)
  assets/generated/visual/r1_1_visual/textures/*.png   (gitignored pixels)
  assets/generated/visual/r1_1_visual/animations/*.json (gitignored clip data)
  analysis/assets/scenes/r1_1_visual.json              (committed metadata)
  analysis/assets/scenes/animations/*.json             (committed clip metadata)
  analysis/assets/scenes/r1_1_visual_manifest.json     (committed provenance)
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import shutil
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import UnityPy
from UnityPy.enums import TextureFormat

from extract_unity import (
    CONVERTER_VERSION,
    REFERENCE_APK_SHA256,
    ROOT,
    UNITYPY_VERSION,
    bundle_path,
    safe_name,
    source_sha256,
)

DEFAULT_BUNDLE_ROOT = ROOT / "analysis/apktool_out/assets/AssetBundles"
DEFAULT_TEXTURE_MANIFEST = ROOT / "analysis/assets/texture_conversion_manifest.json"
DEFAULT_OUT = ROOT / "assets/generated/visual/r1_1_visual"
DEFAULT_SCENE_META = ROOT / "analysis/assets/scenes/r1_1_visual.json"
DEFAULT_SCENE_MANIFEST = ROOT / "analysis/assets/scenes/r1_1_visual_manifest.json"

TEXTURE_SPECS = (
    ("floor_atlas", "res/floor", "5467841708608453938"),
    ("character_atlas", "common", "6092784074628815488"),
    ("object_common_atlas", "common", "3188784419113904678"),
)

TEXTURE_BY_PATH_ID = {path_id: (name, bundle) for name, bundle, path_id in TEXTURE_SPECS}
PLAYER_BODY_PATH = "img/body"
PLAYER_BODY_SCENE_PATH = "c00/img/body"

SORTING_LAYER_NAMES = {
    0: "Default",
    1: "BackGround",
    2: "Floor",
    3: "Shadow",
    4: "Wall",
    5: "Character",
    6: "WallFont",
    7: "Effect",
    8: "UI",
}

# CONFIRMED from MapManagerLevel.<CreateRoom>d__35.MoveNext: rooms are placed
# on a 41-unit map-cell grid.  RGRoomX.OnSetRGRandomSeed centers a 15x15 room
# inside that cell with room_x_offset/room_y_offset = (41 - size) / 2.
ROOM_MAP_CELL_SIZE = 41

# CONFIRMED from RGRoomX.CreateWall: the perimeter wall rows/columns omit the
# five cells x/y=18..22.  These are the five-cell door openings.
WALL_GAP_MIN = 18
WALL_GAP_MAX = 22
DOOR_GAP_CELLS = tuple(range(WALL_GAP_MIN, WALL_GAP_MAX + 1))

# CONFIRMED from serialized r1_1 doors, trigger colliders and RGDoor/RGDoorTrigger.
# aisle_* -> (side, RGAisle.direction, emRoomDoorDir enter/exit value).
AISLE_DIRECTIONS = {
    "aisle_n": ("north", 3, 1),
    "aisle_s": ("south", 1, 2),
    "aisle_e": ("east", 0, 4),
    "aisle_w": ("west", 2, 3),
}

# CONFIRMED closed-door PlatformEffector2D: UseOneWay=true, SurfaceArc=180,
# rotationalOffset 0/180/270/90 for south/north/west/east.  The one-way "top"
# faces into the room, so a closed door blocks the player leaving the room and
# permits entry from the aisle.  This is the native encoding of that normal.
DOOR_ONE_WAY_BLOCK = {
    "south": "negative_y",
    "north": "positive_y",
    "west": "negative_x",
    "east": "positive_x",
}
DOOR_EFFECTOR_ROTATION = {
    "south": 0.0,
    "north": 180.0,
    "west": 270.0,
    "east": 90.0,
}

# CONFIRMED MapManagerLevel level 1-1 wall_list entry: wall601/wall602 both use a
# 1x1 BoxCollider2D at offset (0, 0.5).  This reconstruction renders the wall601
# variant; the reference randomly chooses a variant per cell.
WALL_PREFAB_MANAGER_PATH_ID = "-8892723654509893235"
WALL_PREFAB_VARIANT = 1

# NATIVE TEST CONFIGURATION. The serialized r1_1 entrance array is [0,0,0,0].
# MapManagerLevel assigns real entrance bits from room_relation at runtime.  The
# test scene deliberately opens all four sides so every recovered door/aisle can
# be exercised without reconstructing the surrounding procedural map.
TEST_ENTRANCE = {"east": True, "south": True, "west": True, "north": True}
TEST_DOOR_OPEN = True


def filter_name(value: int) -> str:
    return {0: "point", 1: "bilinear", 2: "trilinear"}.get(int(value), "point")


def wrap_name(value: int) -> str:
    return {0: "repeat", 1: "clamp", 2: "mirror"}.get(int(value), "clamp")


def color_bytes(color) -> list[int]:
    def channel(value: float) -> int:
        return max(0, min(255, int(round(float(value) * 255.0))))

    return [channel(color.r), channel(color.g), channel(color.b), channel(color.a)]


def qangle(rotation) -> float:
    return math.degrees(2.0 * math.atan2(float(rotation.z), float(rotation.w)))


class SceneBuilder:
    def __init__(self, args):
        self.args = args
        # level-objects supplies the runtime wall prefab; level-common supplies
        # the door AnimatorControllers referenced by the room prefab.
        self.env = UnityPy.load(
            str(bundle_path("room/r1_1", args.bundle_root)),
            str(bundle_path("level-objects", args.bundle_root)),
            str(bundle_path("level-common", args.bundle_root)),
            str(bundle_path("res/floor", args.bundle_root)),
            str(bundle_path("common", args.bundle_root)),
        )
        self.sprites: dict[tuple[str, str], dict] = {}
        self.sprite_order: list[str] = []
        self.instances: list[dict] = []
        self.textures: dict[str, dict] = {}
        self.animations: dict[str, dict] = {}
        self.animation_json: dict[str, str] = {}
        self.topology: dict = {}
        self.wall_map: dict = {}
        self.texture_manifest = json.loads(args.texture_manifest.read_text(encoding="utf-8"))
        self.texture_manifest_by_id = {
            entry["source_path_id"]: entry
            for entry in self.texture_manifest.get("conversions", [])
        }

    def texture_info(self, texture_name: str, source_bundle: str, path_id: str) -> dict:
        if texture_name in self.textures:
            return self.textures[texture_name]
        entry = self.texture_manifest_by_id.get(path_id)
        if entry is None:
            raise SystemExit(
                f"texture {texture_name} ({path_id}) is missing from "
                f"{self.args.texture_manifest}; run convert-texture first"
            )
        png_source = ROOT / entry["output_png"]
        if not png_source.exists():
            raise SystemExit(f"converted texture PNG missing: {png_source}")
        texture_object = None
        for obj in self.env.objects:
            if obj.type.name == "Texture2D" and str(obj.path_id) == path_id:
                texture_object = obj.read()
                break
        if texture_object is None:
            raise SystemExit(f"Texture2D path_id {path_id} not found in loaded bundles")

        settings = getattr(texture_object, "m_TextureSettings", None)
        filter_mode = int(getattr(settings, "m_FilterMode", 0) or 0)
        wrap_u = int(getattr(settings, "m_WrapU", 1) or 1)
        texture = {
            "id": texture_name,
            "png": f"textures/{texture_name}.png",
            "source_bundle": source_bundle,
            "source_path_id": path_id,
            "source_name": getattr(texture_object, "m_Name", texture_name),
            "width": int(texture_object.m_Width),
            "height": int(texture_object.m_Height),
            "format": TextureFormat(int(texture_object.m_TextureFormat)).name,
            "filter": filter_name(filter_mode),
            "wrap": wrap_name(wrap_u),
            "rgba8_sha256": entry["rgba8_sha256"],
            "png_sha256": entry["png_sha256"],
        }
        self.textures[texture_name] = texture
        self._copy_texture_png(png_source, texture_name)
        return texture

    def _copy_texture_png(self, source: Path, texture_name: str) -> None:
        target = self.args.out / "textures" / f"{texture_name}.png"
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)

    def sprite_record(self, sprite, texture_name: str, sprite_path_id: str) -> str:
        # Key by the actual Sprite object path ID plus texture/name.
        key = (sprite_path_id, str(getattr(sprite, "m_Name", "")))
        existing = self.sprites.get(key)
        if existing is not None:
            return existing["id"]

        texture = self.textures[texture_name]
        tex_rect = sprite.m_RD.textureRect
        tex_w = int(texture["width"])
        tex_h = int(texture["height"])
        orig_w = float(sprite.m_Rect.width)
        orig_h = float(sprite.m_Rect.height)
        source_pivot = [float(sprite.m_Pivot.x), float(sprite.m_Pivot.y)]
        offset = sprite.m_RD.textureRectOffset
        local_left = -source_pivot[0] * orig_w + float(offset.x)
        local_bottom = -source_pivot[1] * orig_h + float(offset.y)
        if float(tex_rect.width) <= 0.0 or float(tex_rect.height) <= 0.0:
            raise SystemExit(f"sprite {sprite.m_Name} has an empty texture rect")
        pivot = [
            -local_left / float(tex_rect.width),
            -local_bottom / float(tex_rect.height),
        ]
        if not (-0.02 <= pivot[0] <= 1.02 and -0.02 <= pivot[1] <= 1.02):
            raise SystemExit(
                f"sprite {sprite.m_Name} has an out-of-range adjusted pivot {pivot}"
            )
        pivot = [max(0.0, min(1.0, pivot[0])), max(0.0, min(1.0, pivot[1]))]

        record = {
            "id": f"{safe_name(getattr(sprite, 'm_Name', 'sprite'))}_{sprite_path_id}",
            "texture": texture_name,
            "source_name": getattr(sprite, "m_Name", ""),
            "sprite_path_id": str(sprite_path_id),
            "rect": [
                float(tex_rect.x),
                float(tex_h - tex_rect.y - tex_rect.height),
                float(tex_rect.width),
                float(tex_rect.height),
            ],
            "source_texture_rect": [
                float(tex_rect.x),
                float(tex_rect.y),
                float(tex_rect.width),
                float(tex_rect.height),
            ],
            "source_texture_rect_offset": [float(offset.x), float(offset.y)],
            "source_rect": [float(sprite.m_Rect.x), float(sprite.m_Rect.y),
                            float(sprite.m_Rect.width), float(sprite.m_Rect.height)],
            "source_pivot": source_pivot,
            "pivot": pivot,
            "pixels_per_unit": float(sprite.m_PixelsToUnits),
            "border": [float(sprite.m_Border.x), float(sprite.m_Border.y),
                       float(sprite.m_Border.z), float(sprite.m_Border.w)],
            "filter": texture["filter"],
            "wrap": texture["wrap"],
        }
        self.sprites[key] = record
        self.sprite_order.append(record["id"])
        return record["id"]

    def ensure_sprite_asset(self, sprite_ptr) -> tuple[str, object]:
        sprite = sprite_ptr.read()
        texture_path_id = str(sprite.m_RD.texture.path_id)
        spec = TEXTURE_BY_PATH_ID.get(texture_path_id)
        if spec is None:
            raise SystemExit(
                f"unhandled texture path_id {texture_path_id} for {sprite.m_Name}")
        texture_name, source_bundle = spec
        self.texture_info(texture_name, source_bundle, texture_path_id)
        sprite_id = self.sprite_record(sprite, texture_name, str(sprite_ptr.path_id))
        return sprite_id, sprite

    @staticmethod
    def _streamed_frames(clip) -> list[tuple[float, list[tuple[int, float]]]]:
        stream = clip.m_MuscleClip.m_Clip.data.m_StreamedClip
        data = stream.data
        raw = struct.pack(f"<{len(data)}I", *data)
        frames: list[tuple[float, list[tuple[int, float]]]] = []
        position = 0
        while position + 8 <= len(raw):
            time, key_count = struct.unpack_from("<fi", raw, position)
            position += 8
            keys: list[tuple[int, float]] = []
            for _ in range(key_count):
                if position + 20 > len(raw):
                    raise SystemExit("truncated StreamedClip key data")
                curve_index, = struct.unpack_from("<i", raw, position)
                coefficients = struct.unpack_from("<4f", raw, position + 4)
                position += 20
                keys.append((curve_index, coefficients[3]))
            frames.append((time, keys))
            if key_count == 0:
                break
        if position != len(raw):
            raise SystemExit("trailing bytes in StreamedClip data")
        return frames

    @staticmethod
    def _binding_curve_count(binding) -> int:
        if int(getattr(binding, "typeID", 0) or 0) == 4:  # Transform
            if int(binding.attribute) in (1, 3, 4):
                return 3
            if int(binding.attribute) == 2:
                return 4
        return 1

    def extract_player_animations(self, player_root) -> None:
        animator_component = None
        for component in player_root.read().m_Component:
            if component.component.type.name == "Animator":
                animator_component = component.component
                break
        if animator_component is None:
            raise SystemExit("c00 has no Animator component")

        animator = animator_component.read()
        controller_ptr = animator.m_Controller
        if controller_ptr is None or controller_ptr.path_id == 0:
            raise SystemExit("c00 Animator has no controller")
        controller = controller_ptr.read()
        state_machine = controller.m_Controller.m_StateMachineArray[0].data
        state_names = {2215514825: "ide", 1349952704: "run", 3978643748: "dead"}
        selected_states = {"ide": "c00_idle", "run": "c00_run"}

        states_by_name: dict[str, object] = {}
        for state_offset in state_machine.m_StateConstantArray:
            state = state_offset.data
            state_name = state_names.get(int(state.m_NameID))
            if state_name in selected_states:
                states_by_name[state_name] = state

        for state_name, native_id in selected_states.items():
            state = states_by_name.get(state_name)
            if state is None:
                raise SystemExit(f"c00 controller has no selected {state_name} state")
            clip_index = int(
                state.m_BlendTreeConstantArray[0].data.m_NodeArray[0].data.m_ClipID)
            clip_ptr = controller.m_AnimationClips[clip_index]
            clip = clip_ptr.read()

            loop = bool(state.m_Loop) and bool(clip.m_MuscleClip.m_LoopTime)
            duration = float(
                clip.m_MuscleClip.m_StopTime - clip.m_MuscleClip.m_StartTime)
            sample_rate = float(clip.m_SampleRate)

            bindings = list(clip.m_ClipBindingConstant.genericBindings)
            target_hash = zlib.crc32(PLAYER_BODY_PATH.encode("utf-8")) & 0xFFFFFFFF
            target_index = None
            for index, binding in enumerate(bindings):
                if (int(binding.path) == target_hash and
                        int(binding.customType) == 23 and
                        int(binding.isPPtrCurve) == 1 and
                        int(getattr(binding, "typeID", 0) or 0) == 212):
                    target_index = index
                    break
            if target_index is None:
                raise SystemExit(
                    f"no SpriteRenderer.m_Sprite binding for {PLAYER_BODY_PATH}")
            curve_index = sum(
                self._binding_curve_count(b) for b in bindings[:target_index])

            frames = self._streamed_frames(clip)
            if not frames:
                raise SystemExit("animation clip has no streamed frames")
            initial_values = {index: value for index, value in frames[0][1]}
            curve_keys: list[tuple[float, float]] = []
            for time, keys in frames[1:]:
                if not math.isfinite(time) or time < 0.0:
                    continue
                for index, value in keys:
                    if index == curve_index:
                        curve_keys.append((float(time), float(value)))
            if not curve_keys:
                curve_keys = [(0.0, float(initial_values.get(curve_index, 0.0)))]
            elif curve_keys[0][0] > 0.0:
                curve_keys.insert(
                    0, (0.0, float(initial_values.get(curve_index, 0.0))))

            mapping = list(clip.m_ClipBindingConstant.pptrCurveMapping)
            if not mapping:
                raise SystemExit("animation clip pptrCurveMapping is empty")
            mapping_sprite_ids: list[str] = []
            for mapping_ptr in mapping:
                sprite_id, _ = self.ensure_sprite_asset(mapping_ptr)
                mapping_sprite_ids.append(sprite_id)

            source_keys: list[dict] = []
            for time, value in curve_keys:
                rounded = int(round(value))
                if abs(value - rounded) > 1e-4 or not (0 <= rounded < len(mapping)):
                    raise SystemExit(
                        f"pptr curve value {value} does not index pptrCurveMapping")
                source_keys.append({
                    "time": time,
                    "sprite": mapping_sprite_ids[rounded],
                })

            track = {
                "target_path": PLAYER_BODY_SCENE_PATH,
                "property": "m_Sprite",
                "keys": source_keys,
            }
            animation = {
                "id": native_id,
                "duration": duration,
                "sample_rate": sample_rate,
                "loop": loop,
                "update_mode": (
                    "normal" if int(animator.m_UpdateMode) == 0
                    else str(int(animator.m_UpdateMode))),
                "tracks": [track],
                "source": {
                    "apk_sha256": REFERENCE_APK_SHA256,
                    "bundle": "common",
                    "bundle_sha256": source_sha256(
                        bundle_path("common", self.args.bundle_root)),
                    "animator_game_object_path_id": str(player_root.path_id),
                    "animator_component_path_id": str(animator_component.path_id),
                    "animator_enabled": bool(animator.m_Enabled),
                    "animator_update_mode": int(animator.m_UpdateMode),
                    "controller_path_id": str(controller_ptr.path_id),
                    "controller_name": getattr(controller, "m_Name", ""),
                    "state_machine_index": "0",
                    "state_machine_layer": "Base Layer",
                    "state_path_id": str(state.m_PathID),
                    "state_name": state_name,
                    "state_loop": bool(state.m_Loop),
                    "state_speed": float(state.m_Speed),
                    "clip_path_id": str(clip_ptr.path_id),
                    "clip_name": getattr(clip, "m_Name", ""),
                    "clip_start_time": float(clip.m_MuscleClip.m_StartTime),
                    "clip_stop_time": float(clip.m_MuscleClip.m_StopTime),
                    "serialized_pptr_curve_count": len(clip.m_PPtrCurves or []),
                    "pptr_curve_mapping_count": len(mapping),
                    "binding_path": PLAYER_BODY_PATH,
                    "binding_property": "m_Sprite",
                    "binding_path_hash": str(target_hash),
                    "pptr_curve_index": str(curve_index),
                    "sprite_mapping_path_ids": [str(p.path_id) for p in mapping],
                },
            }
            self.animations[native_id] = animation
            self.animation_json[native_id] = (
                json.dumps(animation, indent=2, sort_keys=True) + "\n")

    def add_sprite_renderer(self, sr, world_pos, world_angle, world_scale, path,
                            allowed_names=None) -> None:
        sprite_ptr = sr.m_Sprite
        if sprite_ptr is None or sprite_ptr.path_id == 0:
            return
        sprite = sprite_ptr.read()
        if allowed_names is not None and getattr(sprite, "m_Name", "") not in allowed_names:
            return
        sprite_id, _ = self.ensure_sprite_asset(sprite_ptr)
        self.instances.append({
            "sprite": sprite_id,
            "position": [world_pos[0], world_pos[1]],
            "rotation": math.radians(world_angle),
            "scale": [world_scale[0], world_scale[1]],
            "color": color_bytes(sr.m_Color),
            "sorting_layer": int(sr.m_SortingLayer),
            "sorting_layer_name": SORTING_LAYER_NAMES.get(int(sr.m_SortingLayer), "Unknown"),
            "sorting_order": int(sr.m_SortingOrder),
            "flip_x": bool(sr.m_FlipX),
            "flip_y": bool(sr.m_FlipY),
            "path": path,
        })

    @staticmethod
    def _component(game_object, type_name):
        for component in game_object.m_Component:
            if component.component.type.name == type_name:
                return component.component.read(), component.component
        return None, None

    @staticmethod
    def _rect_from_collider(position, collider):
        center_x = float(position.x) + float(collider.m_Offset.x)
        center_y = float(position.y) + float(collider.m_Offset.y)
        half_x = float(collider.m_Size.x) * 0.5
        half_y = float(collider.m_Size.y) * 0.5
        return [center_x - half_x, center_y - half_y,
                center_x + half_x, center_y + half_y]

    def _wall_prefab(self):
        manager = None
        for obj in self.env.objects:
            if obj.type.name != "MonoBehaviour":
                continue
            if str(obj.path_id) != WALL_PREFAB_MANAGER_PATH_ID:
                continue
            manager = obj.read()
            break
        if manager is None:
            raise SystemExit(
                f"MapManagerLevel {WALL_PREFAB_MANAGER_PATH_ID} not found")
        wall_ptrs = list(manager.wall_list)
        if len(wall_ptrs) <= WALL_PREFAB_VARIANT:
            raise SystemExit("wall_list does not contain the selected variant")
        wall_go = wall_ptrs[WALL_PREFAB_VARIANT].read()
        wall_tf = next(
            c.component for c in wall_go.m_Component
            if c.component.type.name == "Transform").read()
        records = []

        def collect(transform, path):
            go = transform.m_GameObject.read()
            for component in go.m_Component:
                if component.component.type.name != "SpriteRenderer":
                    continue
                sr = component.component.read()
                sprite = sr.m_Sprite.read()
                records.append({
                    "path": f"{path}/{go.m_Name}",
                    "sprite_name": getattr(sprite, "m_Name", ""),
                    "sprite_ptr": sr.m_Sprite,
                    "sorting_layer": int(sr.m_SortingLayer),
                    "sorting_order": int(sr.m_SortingOrder),
                    "color": color_bytes(sr.m_Color),
                    "local_x": float(transform.m_LocalPosition.x),
                    "local_y": float(transform.m_LocalPosition.y),
                })
            for child in transform.m_Children:
                collect(child.read(), f"{path}/{go.m_Name}")

        collect(wall_tf, wall_go.m_Name)
        return records

    def _wall_prefab_info(self):
        manager = None
        for obj in self.env.objects:
            if obj.type.name != "MonoBehaviour":
                continue
            if str(obj.path_id) == WALL_PREFAB_MANAGER_PATH_ID:
                manager = obj.read()
                break
        if manager is None:
            raise SystemExit(
                f"MapManagerLevel {WALL_PREFAB_MANAGER_PATH_ID} not found")
        variants = []
        for index, wall_ptr in enumerate(manager.wall_list):
            wall_go = wall_ptr.read()
            wall_tf = next(
                c.component for c in wall_go.m_Component
                if c.component.type.name == "Transform").read()
            wall_collider, _ = self._component(wall_go, "BoxCollider2D")
            variants.append({
                "index": index,
                "name": wall_go.m_Name,
                "path_id": str(wall_ptr.path_id),
                "serialized_layer": int(wall_go.m_Layer),
                "serialized_layer_name": "Wall",
                "runtime_layer": 17,
                "runtime_layer_name": "Edge",
                "tag": int(wall_go.m_Tag),
                "local_position": [float(wall_tf.m_LocalPosition.x),
                                   float(wall_tf.m_LocalPosition.y)],
                "collider_size": [float(wall_collider.m_Size.x),
                                  float(wall_collider.m_Size.y)],
                "collider_offset": [float(wall_collider.m_Offset.x),
                                    float(wall_collider.m_Offset.y)],
                "collider_trigger": bool(wall_collider.m_IsTrigger),
            })
        return {
            "manager_path_id": WALL_PREFAB_MANAGER_PATH_ID,
            "selected_visual_variant": WALL_PREFAB_VARIANT,
            "variants": variants,
            "runtime_layer": 17,
            "runtime_layer_name": "Edge",
            "classification": (
                "CONFIRMED serialized level-1-1 wall prefabs. The prefabs are "
                "serialized on layer 13 (Wall), but RGRoomX.cctor sets "
                "EdgeLayer=17 and CreateWall/CreateAisleWall assign generated "
                "wall instances to layer 17 (Edge). Deterministic single-variant "
                "rendering in this reconstruction."),
        }

    def _add_wall_instances(self, wall_cells, corridor_cells):
        records = self._wall_prefab()
        all_cells = sorted(set(wall_cells) | set(corridor_cells))
        for (x, y) in all_cells:
            for record in records:
                sprite_id, _ = self.ensure_sprite_asset(record["sprite_ptr"])
                self.instances.append({
                    "sprite": sprite_id,
                    "position": [float(x) + record["local_x"],
                                 float(y) + record["local_y"]],
                    "rotation": 0.0,
                    "scale": [1.0, 1.0],
                    "color": record["color"],
                    "sorting_layer": record["sorting_layer"],
                    "sorting_layer_name": SORTING_LAYER_NAMES.get(
                        record["sorting_layer"], "Unknown"),
                    "sorting_order": record["sorting_order"],
                    "flip_x": False,
                    "flip_y": False,
                    "path": f"native_wall/{record['path']}@{x},{y}",
                })

    @staticmethod
    def _wall_cell_rect(cell):
        x, y = cell
        return [float(x) - 0.5, float(y), float(x) + 0.5, float(y) + 1.0]

    def _extract_topology(self, room_root, room_transform):
        room_go = room_root.read()
        room_tf = room_transform.read()
        room_mb = None
        for component in room_go.m_Component:
            if component.component.type.name != "MonoBehaviour":
                continue
            candidate = component.component.read()
            if hasattr(candidate, "room_width") and hasattr(candidate, "entrance"):
                room_mb = candidate
                break
        if room_mb is None:
            raise SystemExit("r1_1 has no RGRoomX MonoBehaviour")

        width = int(room_mb.room_width)
        height = int(room_mb.room_height)
        offset_x = (ROOM_MAP_CELL_SIZE - width) // 2
        offset_y = (ROOM_MAP_CELL_SIZE - height) // 2
        if width != 15 or height != 15:
            raise SystemExit(
                f"unexpected r1_1 room size {width}x{height}; evidence expects 15x15")
        if offset_x != 13 or offset_y != 13:
            raise SystemExit(
                f"unexpected r1_1 map offsets ({offset_x},{offset_y})")

        aisle_children = {}
        for child in room_tf.m_Children:
            transform = child.read()
            game_object = transform.m_GameObject.read()
            name = getattr(game_object, "m_Name", "")
            if name in AISLE_DIRECTIONS:
                aisle_children[name] = (transform, game_object)

        openings = []
        for aisle_name, (side, aisle_direction, exit_direction) in AISLE_DIRECTIONS.items():
            if aisle_name not in aisle_children:
                raise SystemExit(f"r1_1 missing {aisle_name}")
            aisle_tf, aisle_go = aisle_children[aisle_name]
            door_tf = None
            trigger_go = None
            trigger_gameobject_path_id = None
            for child in aisle_tf.m_Children:
                transform = child.read()
                game_object = transform.m_GameObject.read()
                if getattr(game_object, "m_Name", "") == "door":
                    door_tf = transform
            if door_tf is None:
                raise SystemExit(f"{aisle_name} is missing door")
            for child in door_tf.m_Children:
                transform = child.read()
                game_object = transform.m_GameObject.read()
                if getattr(game_object, "m_Name", "") == "door_trigger":
                    trigger_go = game_object
                    trigger_gameobject_path_id = str(
                        transform.m_GameObject.path_id)
            if trigger_go is None:
                raise SystemExit(f"{aisle_name} is missing door_trigger")
            door_go = door_tf.m_GameObject.read()
            door_collider, door_collider_ptr = self._component(
                door_go, "BoxCollider2D")
            door_behaviour, door_behaviour_ptr = self._component(
                door_go, "MonoBehaviour")
            animator, animator_ptr = self._component(door_go, "Animator")
            effector, effector_ptr = self._component(
                door_go, "PlatformEffector2D")
            trigger_collider, trigger_collider_ptr = self._component(
                trigger_go, "BoxCollider2D")
            trigger_behaviour, trigger_behaviour_ptr = self._component(
                trigger_go, "MonoBehaviour")
            if None in (door_collider, trigger_collider):
                raise SystemExit(f"{aisle_name} door collider is missing")
            if door_collider.m_IsTrigger or not trigger_collider.m_IsTrigger:
                raise SystemExit(f"{aisle_name} door/trigger flags are unexpected")
            if effector is None or not effector.m_UseOneWay:
                raise SystemExit(f"{aisle_name} door is missing its one-way effector")
            if abs(float(effector.m_SurfaceArc) - 180.0) > 1e-4:
                raise SystemExit(f"{aisle_name} door one-way arc is unexpected")
            if abs(float(effector.m_RotationalOffset) -
                   DOOR_EFFECTOR_ROTATION[side]) > 1e-4:
                raise SystemExit(
                    f"{aisle_name} door one-way rotation is unexpected")

            controller_name = None
            clip_names = []
            if animator is not None and animator.m_Controller:
                controller = animator.m_Controller.read()
                controller_name = getattr(controller, "m_Name", None)
                for clip_ptr in controller.m_AnimationClips:
                    clip_names.append(getattr(clip_ptr.read(), "m_Name", ""))

            door_position = door_tf.m_LocalPosition
            barrier = self._rect_from_collider(door_position, door_collider)
            trigger = self._rect_from_collider(door_position, trigger_collider)
            enabled = bool(TEST_ENTRANCE[side])
            is_open = bool(TEST_DOOR_OPEN and enabled)
            openings.append({
                "side": side,
                "aisle": aisle_name,
                "aisle_direction": aisle_direction,
                "enter_exit_direction": exit_direction,
                "aisle_path_id": str(aisle_tf.m_GameObject.path_id),
                "door_path_id": str(door_tf.m_GameObject.path_id),
                "door_behaviour_path_id": str(door_behaviour_ptr.path_id),
                "trigger_path_id": trigger_gameobject_path_id,
                "trigger_behaviour_path_id": str(trigger_behaviour_ptr.path_id),
                "animator_path_id": str(animator_ptr.path_id),
                "animator_controller_path_id": (
                    str(animator.m_Controller.path_id)
                    if animator is not None and animator.m_Controller else None),
                "animator_controller_name": controller_name,
                "animation_clip_names": clip_names,
                "position": [float(door_position.x), float(door_position.y)],
                "rotation": [float(door_tf.m_LocalRotation.x),
                             float(door_tf.m_LocalRotation.y),
                             float(door_tf.m_LocalRotation.z),
                             float(door_tf.m_LocalRotation.w)],
                "scale": [float(door_tf.m_LocalScale.x),
                          float(door_tf.m_LocalScale.y),
                          float(door_tf.m_LocalScale.z)],
                "collider_size": [float(door_collider.m_Size.x),
                                  float(door_collider.m_Size.y)],
                "collider_offset": [float(door_collider.m_Offset.x),
                                    float(door_collider.m_Offset.y)],
                "collider_trigger": bool(door_collider.m_IsTrigger),
                "trigger_size": [float(trigger_collider.m_Size.x),
                                 float(trigger_collider.m_Size.y)],
                "trigger_offset": [float(trigger_collider.m_Offset.x),
                                   float(trigger_collider.m_Offset.y)],
                "trigger_is_trigger": bool(trigger_collider.m_IsTrigger),
                "effector_use_one_way": bool(effector.m_UseOneWay),
                "effector_surface_arc": float(effector.m_SurfaceArc),
                "effector_rotational_offset": float(effector.m_RotationalOffset),
                "one_way_block": DOOR_ONE_WAY_BLOCK[side],
                "barrier": barrier,
                "trigger": trigger,
                "entrance": enabled,
                "door_open": is_open,
            })

        room_walls = set()
        for y in (offset_y - 1, offset_y + height):
            side = "south" if y == offset_y - 1 else "north"
            for x in range(offset_x - 1, offset_x + width + 1):
                if (not TEST_ENTRANCE[side] and WALL_GAP_MIN <= x <= WALL_GAP_MAX):
                    room_walls.add((x, y))
                elif not (WALL_GAP_MIN <= x <= WALL_GAP_MAX):
                    room_walls.add((x, y))
        for x in (offset_x - 1, offset_x + width):
            side = "west" if x == offset_x - 1 else "east"
            for y in range(offset_y, offset_y + height):
                if (not TEST_ENTRANCE[side] and WALL_GAP_MIN <= y <= WALL_GAP_MAX):
                    room_walls.add((x, y))
                elif not (WALL_GAP_MIN <= y <= WALL_GAP_MAX):
                    room_walls.add((x, y))

        corridor_walls = set()
        if TEST_ENTRANCE["west"]:
            for x in range(0, offset_x - 1):
                corridor_walls.update({(x, 17), (x, 23)})
        if TEST_ENTRANCE["east"]:
            for x in range(offset_x + width + 1, ROOM_MAP_CELL_SIZE):
                corridor_walls.update({(x, 17), (x, 23)})
        if TEST_ENTRANCE["south"]:
            for y in range(0, offset_y - 1):
                corridor_walls.update({(17, y), (23, y)})
        if TEST_ENTRANCE["north"]:
            for y in range(offset_y + height + 1, ROOM_MAP_CELL_SIZE):
                corridor_walls.update({(17, y), (23, y)})

        floor_cells = [(x, y) for y in range(offset_y, offset_y + height)
                       for x in range(offset_x, offset_x + width)]
        aisle_cells = set()
        if TEST_ENTRANCE["west"]:
            aisle_cells.update((x, y) for x in range(0, offset_x - 1)
                               for y in DOOR_GAP_CELLS)
        if TEST_ENTRANCE["east"]:
            aisle_cells.update((x, y) for x in range(offset_x + width + 1,
                                                      ROOM_MAP_CELL_SIZE)
                               for y in DOOR_GAP_CELLS)
        if TEST_ENTRANCE["south"]:
            aisle_cells.update((x, y) for y in range(0, offset_y - 1)
                               for x in DOOR_GAP_CELLS)
        if TEST_ENTRANCE["north"]:
            aisle_cells.update((x, y) for y in range(offset_y + height + 1,
                                                      ROOM_MAP_CELL_SIZE)
                               for x in DOOR_GAP_CELLS)
        room_center = [float(offset_x) + float(width - 1) * 0.5,
                       float(offset_y) + float(height) * 0.5]
        wall_prefabs = self._wall_prefab_info()

        self._add_wall_instances(sorted(room_walls), sorted(corridor_walls))
        for instance in self.instances:
            path = instance.get("path", "")
            for aisle_name, (side, _, _) in AISLE_DIRECTIONS.items():
                if f"/{aisle_name}/door/" in path:
                    instance["door"] = side
                    break

        solids = [self._wall_cell_rect(cell)
                  for cell in sorted(room_walls | corridor_walls)]
        opening_by_side = {opening["side"]: opening for opening in openings}
        collision = {
            "bounds": [0.0, 0.0,
                       float(ROOM_MAP_CELL_SIZE), float(ROOM_MAP_CELL_SIZE)],
            "bounds_classification": (
                "NATIVE TEST BOUNDARY: outer edge of the confirmed 41-unit "
                "MapManagerLevel room cell; the surrounding procedural map is "
                "outside this reconstruction"),
            "interior_bounds": [float(offset_x) - 0.5, float(offset_y),
                                float(width), float(height)],
            "solids": solids,
            "solid_count": len(solids),
            "doors": [
                {
                    "id": opening["side"],
                    "aisle": opening["aisle"],
                    "direction": opening["aisle_direction"],
                    "enter_exit_direction": opening["enter_exit_direction"],
                    "door_path": opening["door_path_id"],
                    "door_behaviour": opening["door_behaviour_path_id"],
                    "trigger_path": opening["trigger_path_id"],
                    "trigger_behaviour": opening["trigger_behaviour_path_id"],
                    "position": opening["position"],
                    "barrier": opening["barrier"],
                    "trigger": opening["trigger"],
                    "one_way_block": opening["one_way_block"],
                    "enabled": opening["entrance"],
                    "open": opening["door_open"],
                    "classification": (
                        "CONFIRMED serialized collider/trigger geometry; "
                        "NATIVE TEST CONFIGURATION open state"),
                }
                for opening in openings
            ],
            "test_configuration": {
                "entrance": TEST_ENTRANCE,
                "door_open": TEST_DOOR_OPEN,
                "classification": (
                    "NATIVE TEST CONFIGURATION: r1_1 serializes entrance "
                    "[0,0,0,0]; MapManagerLevel overwrites entrance from "
                    "room_relation. All four sides are opened here only to "
                    "exercise the recovered door/aisle geometry."),
            },
            "classification": (
                "CONFIRMED CreateWall wall cells, wall-prefab collider, "
                "generated-wall runtime layer 17 (Edge), door/trigger "
                "colliders; NATIVE TEST entrance/door state and map-cell "
                "boundary"),
        }

        topology = {
            "source_room": "room/r1_1",
            "source_wall_manager_path_id": WALL_PREFAB_MANAGER_PATH_ID,
            "create_wall": {
                "room_width": width,
                "room_height": height,
                "map_cell_size": ROOM_MAP_CELL_SIZE,
                "room_x_offset": offset_x,
                "room_y_offset": offset_y,
                "wall_cell_size": [1.0, 1.0],
                "wall_collider_offset": [0.0, 0.5],
                "serialized_wall_prefab_layer": 13,
                "runtime_wall_layer": 17,
                "runtime_wall_layer_name": "Edge",
                "edge_layer_initialization": (
                    "RGRoomX.cctor sets RGRoomX.EdgeLayer = 17; "
                    "CreateWall/CreateAisleWall assign it to generated walls"),
                "horizontal_wall_rows": [offset_y - 1, offset_y + height],
                "vertical_wall_columns": [offset_x - 1, offset_x + width],
                "door_gap_cells": [WALL_GAP_MIN, WALL_GAP_MAX],
                "serialized_entrance": [int(v) for v in room_mb.entrance],
                "serialized_door_open": bool(room_mb.door_open),
                "serialized_create_with_close_state": bool(
                    room_mb.create_with_close_state),
                "classification": (
                    "CONFIRMED RGRoomX.CreateWall and RGRoomX.OnSetRGRandomSeed "
                    "disassembly; serialized r1_1 dimensions/offsets"),
            },
            "openings": openings,
            "floor_cells": [list(cell) for cell in floor_cells],
            "aisle_cells": [list(cell) for cell in sorted(aisle_cells)],
            "room_center": room_center,
            "wall_prefabs": wall_prefabs,
            "wall_cell_count": len(room_walls),
            "corridor_wall_cell_count": len(corridor_walls),
            "floor_cell_count": len(floor_cells),
            "aisle_cell_count": len(aisle_cells),
            "wall_cells": [list(cell) for cell in sorted(room_walls)],
            "corridor_wall_cells": [list(cell) for cell in sorted(corridor_walls)],
            "classification": (
                "CONFIRMED geometry from CreateWall/CreateAisleWall and "
                "serialized r1_1; entrance and door_open runtime state are "
                "NATIVE TEST CONFIGURATION"),
        }

        wall_map = {
            "schema": "soulknight.r1_1_wall_map/v1",
            "room": {
                "width": width,
                "height": height,
                "map_cell_size": ROOM_MAP_CELL_SIZE,
                "offset": [offset_x, offset_y],
                "interior_bounds": collision["interior_bounds"],
                "serialized_entrance": [int(v) for v in room_mb.entrance],
                "serialized_door_open": bool(room_mb.door_open),
            },
            "wall_cells": [list(cell) for cell in sorted(room_walls)],
            "corridor_wall_cells": [list(cell) for cell in sorted(corridor_walls)],
            "floor_cells": [list(cell) for cell in floor_cells],
            "aisle_cells": [list(cell) for cell in sorted(aisle_cells)],
            "room_center": room_center,
            "wall_prefabs": wall_prefabs,
            "openings": openings,
            "test_configuration": collision["test_configuration"],
            "classification": collision["classification"],
        }
        wall_map["ascii"] = self._wall_map_ascii(
            width, height, offset_x, offset_y,
            room_walls, corridor_walls)
        self.topology = topology
        self.wall_map = wall_map
        return collision

    @staticmethod
    def _wall_map_ascii(width, height, offset_x, offset_y,
                        room_walls, corridor_walls):
        grid = [[" " for _ in range(ROOM_MAP_CELL_SIZE)]
                for _ in range(ROOM_MAP_CELL_SIZE)]
        for y in range(offset_y, offset_y + height):
            for x in range(offset_x, offset_x + width):
                grid[y][x] = "."
        for (x, y) in room_walls:
            if 0 <= x < ROOM_MAP_CELL_SIZE and 0 <= y < ROOM_MAP_CELL_SIZE:
                grid[y][x] = "#"
        for (x, y) in corridor_walls:
            if 0 <= x < ROOM_MAP_CELL_SIZE and 0 <= y < ROOM_MAP_CELL_SIZE:
                grid[y][x] = "+"
        for side, cells in (
            ("south", [(x, offset_y - 1) for x in DOOR_GAP_CELLS]),
            ("north", [(x, offset_y + height) for x in DOOR_GAP_CELLS]),
            ("west", [(offset_x - 1, y) for y in DOOR_GAP_CELLS]),
            ("east", [(offset_x + width, y) for y in DOOR_GAP_CELLS]),
        ):
            if TEST_ENTRANCE[side]:
                for (x, y) in cells:
                    grid[y][x] = "D"
        for side, cells in (
            ("south", [(x, y) for y in range(0, offset_y - 1)
                       for x in DOOR_GAP_CELLS]),
            ("north", [(x, y) for y in range(offset_y + height + 1,
                                             ROOM_MAP_CELL_SIZE)
                       for x in DOOR_GAP_CELLS]),
            ("west", [(x, y) for x in range(0, offset_x - 1)
                      for y in DOOR_GAP_CELLS]),
            ("east", [(x, y) for x in range(offset_x + width + 1,
                                             ROOM_MAP_CELL_SIZE)
                      for y in DOOR_GAP_CELLS]),
        ):
            if TEST_ENTRANCE[side]:
                for (x, y) in cells:
                    grid[y][x] = ":"
        lines = ["".join(row) for row in reversed(grid)]
        return "\n".join(lines) + "\n"

    def walk_room(self, tf_ptr, parent_pos, parent_angle, parent_scale, path):
        transform = tf_ptr.read()
        game_object = transform.m_GameObject.read()
        name = getattr(game_object, "m_Name", "")
        local_position = transform.m_LocalPosition
        local_rotation = transform.m_LocalRotation
        local_scale = transform.m_LocalScale
        angle = qangle(local_rotation)
        cos_a = math.cos(math.radians(parent_angle))
        sin_a = math.sin(math.radians(parent_angle))
        world_x = parent_pos[0] + (
            cos_a * float(local_position.x) * parent_scale[0] -
            sin_a * float(local_position.y) * parent_scale[1]
        )
        world_y = parent_pos[1] + (
            sin_a * float(local_position.x) * parent_scale[0] +
            cos_a * float(local_position.y) * parent_scale[1]
        )
        world_angle = parent_angle + angle
        world_scale = (
            parent_scale[0] * float(local_scale.x),
            parent_scale[1] * float(local_scale.y),
        )
        current_path = f"{path}/{name}"
        for component in game_object.m_Component:
            if component.component.type.name == "SpriteRenderer":
                sr = component.component.read()
                if getattr(sr, "m_Enabled", True):
                    self.add_sprite_renderer(
                        sr, (world_x, world_y), world_angle, world_scale, current_path)
        for child in transform.m_Children:
            self.walk_room(child, (world_x, world_y), world_angle, world_scale, current_path)

    def walk_player(self, tf_ptr, parent_pos, parent_angle, parent_scale, path):
        transform = tf_ptr.read()
        game_object = transform.m_GameObject.read()
        name = getattr(game_object, "m_Name", "")
        local_position = transform.m_LocalPosition
        local_rotation = transform.m_LocalRotation
        local_scale = transform.m_LocalScale
        angle = qangle(local_rotation)
        cos_a = math.cos(math.radians(parent_angle))
        sin_a = math.sin(math.radians(parent_angle))
        world_x = parent_pos[0] + (
            cos_a * float(local_position.x) * parent_scale[0] -
            sin_a * float(local_position.y) * parent_scale[1]
        )
        world_y = parent_pos[1] + (
            sin_a * float(local_position.x) * parent_scale[0] +
            cos_a * float(local_position.y) * parent_scale[1]
        )
        world_angle = parent_angle + angle
        world_scale = (
            parent_scale[0] * float(local_scale.x),
            parent_scale[1] * float(local_scale.y),
        )
        current_path = f"{path}/{name}"
        # Keep the player visual set narrow: body and shadow only.
        if name in ("body", "shadow"):
            for component in game_object.m_Component:
                if component.component.type.name == "SpriteRenderer":
                    sr = component.component.read()
                    if getattr(sr, "m_Enabled", True):
                        self.add_sprite_renderer(
                            sr, (world_x, world_y), world_angle, world_scale,
                            current_path, {"c01_5", "shadow3"})
        for child in transform.m_Children:
            self.walk_player(child, (world_x, world_y), world_angle, world_scale, current_path)

    def build(self):
        room_root = None
        player_root = None
        for obj in self.env.objects:
            if obj.type.name != "GameObject":
                continue
            data = obj.read()
            if getattr(data, "m_Name", "") == "r1_1":
                room_root = obj
            elif getattr(data, "m_Name", "") == "c00":
                components = [c.component.type.name for c in data.m_Component]
                if "MonoBehaviour" in components and player_root is None:
                    player_root = obj
        if room_root is None:
            raise SystemExit("r1_1 root GameObject not found")
        if player_root is None:
            raise SystemExit("c00 root GameObject not found")

        room_transform = next(
            c.component for c in room_root.read().m_Component
            if c.component.type.name == "Transform")
        self.walk_room(room_transform, (0.0, 0.0), 0.0, (1.0, 1.0), "r1_1")
        collision = self._extract_topology(room_root, room_transform)

        player_transform = next(
            c.component for c in player_root.read().m_Component
            if c.component.type.name == "Transform").read()
        spawn = self.args.spawn
        for child in player_transform.m_Children:
            self.walk_player(child, spawn, 0.0, (1.0, 1.0), "c00")
        self.extract_player_animations(player_root)

        return {
            "id": "r1_1_visual",
            "source_room": "room/r1_1",
            "source_player": "common/c00",
            "spawn": [spawn[0], spawn[1]],
            "camera": {
                "position": [spawn[0], spawn[1]],
                "orthographic_size": 10.5,
                "viewport": [1920.0, 1080.0],
            },
            "textures": [self.textures[key] for key in
                         ("floor_atlas", "character_atlas", "object_common_atlas")
                         if key in self.textures],
            "sprites": [self.sprites[key] for key in self.sprites],
            "instances": self.instances,
            "topology": self.topology,
            "collision": collision,
            "animations": [
                f"animations/{native_id}.json"
                for native_id in ("c00_idle", "c00_run")
                if native_id in self.animations
            ],
        }


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle-root", type=Path, default=DEFAULT_BUNDLE_ROOT)
    parser.add_argument("--texture-manifest", type=Path, default=DEFAULT_TEXTURE_MANIFEST)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--scene", type=Path, default=DEFAULT_SCENE_META)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_SCENE_MANIFEST)
    parser.add_argument(
        "--wall-map", type=Path,
        default=ROOT / "analysis/re/r1_1_wall_map.json")
    parser.add_argument("--spawn", type=float, nargs=2, default=(20.0, 20.0))
    args = parser.parse_args(argv)

    builder = SceneBuilder(args)
    scene = builder.build()
    args.out.mkdir(parents=True, exist_ok=True)
    scene_text = json.dumps(scene, indent=2, sort_keys=True) + "\n"
    (args.out / "scene.json").write_text(scene_text, encoding="utf-8")
    args.scene.parent.mkdir(parents=True, exist_ok=True)
    args.scene.write_text(scene_text, encoding="utf-8")

    wall_map_text = json.dumps(builder.wall_map, indent=2, sort_keys=True) + "\n"
    args.wall_map.parent.mkdir(parents=True, exist_ok=True)
    args.wall_map.write_text(wall_map_text, encoding="utf-8")
    wall_map_txt = args.wall_map.with_suffix(".txt")
    wall_map_txt.write_text(
        "r1_1 recovered room topology (ASCII verification map)\n"
        "Legend: # room wall  + corridor wall  D five-cell door opening  "
        "':' corridor floor  '.' room floor\n"
        "Each cell is one world unit; coordinates are integer cell centers.\n\n" +
        builder.wall_map["ascii"],
        encoding="utf-8")

    animation_records: list[dict] = []
    for native_id, animation_json in builder.animation_json.items():
        animation_generated = args.out / "animations" / f"{native_id}.json"
        animation_metadata = args.scene.parent / "animations" / f"{native_id}.json"
        animation_generated.parent.mkdir(parents=True, exist_ok=True)
        animation_metadata.parent.mkdir(parents=True, exist_ok=True)
        animation_generated.write_text(animation_json, encoding="utf-8")
        animation_metadata.write_text(animation_json, encoding="utf-8")
        animation_records.append({
            "id": native_id,
            "metadata": str(animation_metadata.relative_to(ROOT)),
            "generated": str(animation_generated.relative_to(ROOT)),
            "sha256": hashlib.sha256(animation_json.encode("utf-8")).hexdigest(),
        })

    scene_sha = hashlib.sha256(scene_text.encode("utf-8")).hexdigest()
    wall_map_sha = hashlib.sha256(wall_map_text.encode("utf-8")).hexdigest()
    bundles = {
        "room/r1_1": source_sha256(bundle_path("room/r1_1", args.bundle_root)),
        "level-objects": source_sha256(
            bundle_path("level-objects", args.bundle_root)),
        "level-common": source_sha256(
            bundle_path("level-common", args.bundle_root)),
        "res/floor": source_sha256(bundle_path("res/floor", args.bundle_root)),
        "common": source_sha256(bundle_path("common", args.bundle_root)),
    }
    manifest = {
        "manifest_version": 1,
        "tool": "tools/asset/build_visual_scene.py",
        "converter_version": CONVERTER_VERSION,
        "unitypy_version": UNITYPY_VERSION,
        "source_apk_sha256": REFERENCE_APK_SHA256,
        "source_bundles": bundles,
        "scene_metadata": str(args.scene.relative_to(ROOT)),
        "scene_json_sha256": scene_sha,
        "textures": scene["textures"],
        "sprite_count": len(scene["sprites"]),
        "instance_count": len(scene["instances"]),
        "camera": scene["camera"],
        "spawn": scene["spawn"],
        "animation_count": len(scene["animations"]),
        "animation_keyframe_count": sum(
            len(track["keys"])
            for animation in builder.animations.values()
            for track in animation.get("tracks", [])),
        "animations": animation_records,
        "topology": {
            "wall_cell_count": scene["topology"]["wall_cell_count"],
            "corridor_wall_cell_count": scene["topology"]["corridor_wall_cell_count"],
            "floor_cell_count": scene["topology"]["floor_cell_count"],
            "aisle_cell_count": scene["topology"]["aisle_cell_count"],
            "opening_count": len(scene["topology"]["openings"]),
            "wall_map_json": str(args.wall_map.relative_to(ROOT)),
            "wall_map_txt": str(wall_map_txt.relative_to(ROOT)),
            "wall_map_sha256": wall_map_sha,
        },
        "collision": scene["collision"],
        "notes": "r1_1 native test reconstruction with recovered CreateWall/door geometry and c00 idle/run clips; no proprietary pixels committed.",
    }
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(f"scene: {args.out / 'scene.json'}")
    print(f"metadata: {args.scene}")
    print(f"manifest: {args.manifest}")
    print(f"textures: {len(scene['textures'])}, sprites: {len(scene['sprites'])}, "
          f"instances: {len(scene['instances'])}, animations: {len(scene['animations'])}")
    print(f"topology: room_walls={scene['topology']['wall_cell_count']}, "
          f"corridor_walls={scene['topology']['corridor_wall_cell_count']}, "
          f"openings={len(scene['topology']['openings'])}")
    for record in animation_records:
        print(f"{record['id']}_sha256={record['sha256']}")
    print(f"scene_sha256={scene_sha}")
    print(f"wall_map_sha256={wall_map_sha}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
