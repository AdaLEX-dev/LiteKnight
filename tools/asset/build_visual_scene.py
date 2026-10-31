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
        self.env = UnityPy.load(
            str(bundle_path("room/r1_1", args.bundle_root)),
            str(bundle_path("res/floor", args.bundle_root)),
            str(bundle_path("common", args.bundle_root)),
        )
        self.sprites: dict[tuple[str, str], dict] = {}
        self.sprite_order: list[str] = []
        self.instances: list[dict] = []
        self.textures: dict[str, dict] = {}
        self.animations: dict[str, dict] = {}
        self.animation_json: dict[str, str] = {}
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
            "collision": {
                # NATIVE DESIGN: 15x15 recovered room dimensions with a 1-unit
                # wall ring give a 13x13 interior playable AABB.
                "bounds": [spawn[0] - 6.5, spawn[1] - 6.5, 13.0, 13.0],
                "room_width": 15.0,
                "room_height": 15.0,
                "wall_thickness": 1.0,
                "classification": "NATIVE DESIGN from recovered room dimensions",
            },
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
    parser.add_argument("--spawn", type=float, nargs=2, default=(20.0, 20.0))
    args = parser.parse_args(argv)

    builder = SceneBuilder(args)
    scene = builder.build()
    args.out.mkdir(parents=True, exist_ok=True)
    scene_text = json.dumps(scene, indent=2, sort_keys=True) + "\n"
    (args.out / "scene.json").write_text(scene_text, encoding="utf-8")
    args.scene.parent.mkdir(parents=True, exist_ok=True)
    args.scene.write_text(scene_text, encoding="utf-8")

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
    bundles = {
        "room/r1_1": source_sha256(bundle_path("room/r1_1", args.bundle_root)),
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
        "collision": scene["collision"],
        "notes": "r1_1 native test reconstruction with c00 idle/run clips; no proprietary pixels committed.",
    }
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(f"scene: {args.out / 'scene.json'}")
    print(f"metadata: {args.scene}")
    print(f"manifest: {args.manifest}")
    print(f"textures: {len(scene['textures'])}, sprites: {len(scene['sprites'])}, "
          f"instances: {len(scene['instances'])}, animations: {len(scene['animations'])}")
    for record in animation_records:
        print(f"{record['id']}_sha256={record['sha256']}")
    print(f"scene_sha256={scene_sha}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
