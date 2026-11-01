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
import re
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
    ("charactor_common_atlas", "res/charactor_common", "2755411589560438748"),
    ("object_common_group0_atlas", "common", "-1240183975920528313"),
    ("object_level_atlas", "level-common", "8607965889740011037"),
)

TEXTURE_BY_PATH_ID = {path_id: (name, bundle) for name, bundle, path_id in TEXTURE_SPECS}
PLAYER_BODY_PATH = "img/body"
PLAYER_BODY_SCENE_PATH = "c00/img/body"

# English player-facing names recovered from the reference WeaponInfo table
# (Assets/.../WeaponInfo TextAsset) and its localization keys
# weapon/weapon_XXX. Internal prefab IDs remain stable.
WEAPON_DISPLAY_NAMES = {
    "weapon_000": "Bad Pistol",
    "weapon_001": "P250 Pistol",
    "weapon_002": "AK-47",
    "weapon_003": "Shotgun",
    "weapon_004": "Shotgun Pro",
    "weapon_007": "Snow Fox L",
    "weapon_010": "Desert Eagle",
    "weapon_015": "Revolver",
    "weapon_017": "Assault Rifle",
    "weapon_021": "Blaster",
    "weapon_034": "Glacier",
    "weapon_048": "Next-gen SMG",
    "weapon_049": "Next-next-gen SMG",
    "weapon_062": "Machete",
    "weapon_287": "Revolver Gold",
}


def _load_weapon_catalogue() -> dict[str, dict]:
    path = ROOT / "analysis/assets/weapon_catalogue.json"
    if not path.is_file():
        return {}
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return {
        str(entry.get("id")): entry
        for entry in document.get("weapons", [])
        if isinstance(entry, dict) and entry.get("id")
    }


WEAPON_CATALOGUE = _load_weapon_catalogue()


def _load_character_loadouts() -> tuple[dict[str, dict], dict[str, str]]:
    path = ROOT / "analysis/assets/characters/character_loadouts.json"
    if not path.is_file():
        return {}, {}
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}, {}
    entries = {
        str(entry.get("id")): entry
        for entry in document.get("characters", [])
        if isinstance(entry, dict) and entry.get("id")
    }
    names = {}
    for entry in entries.values():
        base = str(entry.get("starting_weapon", ""))
        if base:
            names[base] = str(entry.get("starting_weapon_name", base))
        for variant in entry.get("variants", []) or []:
            if isinstance(variant, dict) and variant.get("id"):
                names[str(variant["id"])] = str(
                    variant.get("display_name", variant["id"]))
    return entries, names


CHARACTER_LOADOUTS, LOADOUT_WEAPON_NAMES = _load_character_loadouts()

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
        self.room_id = getattr(args, "room", "r1_1")
        requested_bundle = getattr(args, "bundle", None)
        if requested_bundle:
            self.bundle_name = str(requested_bundle)
        else:
            self.bundle_name = f"room/{self.room_id}"
        self.root_name = getattr(args, "root_name", None) or self.room_id
        bundle_paths = [
            str(bundle_path("level-objects", args.bundle_root)),
            str(bundle_path("level-common", args.bundle_root)),
            str(bundle_path("res/floor", args.bundle_root)),
            str(bundle_path("res/charactor_common", args.bundle_root)),
            str(bundle_path("common", args.bundle_root)),
        ]
        if Path(bundle_path(self.bundle_name, args.bundle_root)).is_file():
            bundle_paths.insert(0, str(bundle_path(self.bundle_name,
                                                   args.bundle_root)))
        self.env = UnityPy.load(*bundle_paths)
        self.sprites: dict[tuple[str, str], dict] = {}
        self.sprite_order: list[str] = []
        self.instances: list[dict] = []
        self.object_markers: list[dict] = []
        self.characters: list[dict] = []
        self.obstacles: dict = {}
        self.player_stats: dict = {}
        self.weapons: list[dict] = []
        self.enemy_definitions: list[dict] = []
        self.pet_definitions: list[dict] = []
        self.enemy_fixtures: list[dict] = []
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
        self.bundle_cab_names: dict[str, set[str]] = {}
        self.cab_to_bundle: dict[str, str] = {}
        for bundle in self.env.files.values():
            bundle_name = str(getattr(bundle, "name", ""))
            files = getattr(bundle, "files", {})
            if isinstance(files, dict):
                self.bundle_cab_names[bundle_name] = {
                    cab for cab in files
                    if not cab.endswith((".resS", ".resource"))
                }
                for cab in self.bundle_cab_names[bundle_name]:
                    self.cab_to_bundle[cab] = bundle_name

    def _find_texture(self, path_id: str, source_bundle: str):
        expected_cabs = self.bundle_cab_names.get(source_bundle, set())
        for obj in self.env.objects:
            if obj.type.name != "Texture2D" or str(obj.path_id) != path_id:
                continue
            if expected_cabs and obj.assets_file.name not in expected_cabs:
                continue
            return obj
        # Fall back to any matching object for older bundles without a known
        # bundle name mapping.
        for obj in self.env.objects:
            if obj.type.name == "Texture2D" and str(obj.path_id) == path_id:
                return obj
        return None

    def texture_info(self, texture_name: str, source_bundle: str, path_id: str) -> dict:
        if texture_name in self.textures:
            return self.textures[texture_name]
        entry = self.texture_manifest_by_id.get(path_id)
        texture_object_reader = self._find_texture(path_id, source_bundle)
        if texture_object_reader is None:
            raise SystemExit(
                f"Texture2D path_id {path_id} not found in bundle {source_bundle}")

        if entry is None:
            # Scene-specific sprite-atlas textures are not part of the general
            # texture conversion profile. Convert them with UnityPy's decoder
            # into the gitignored generated output so the runtime has pixels;
            # the committed manifest stores only provenance, never image bytes.
            texture_object = texture_object_reader.read()
            image = texture_object.image.convert("RGBA")
            target = self.args.out / "textures" / f"{texture_name}.png"
            target.parent.mkdir(parents=True, exist_ok=True)
            image.save(target)
            rgba = image.tobytes()
            settings = getattr(texture_object, "m_TextureSettings", None)
            filter_mode = int(getattr(settings, "m_FilterMode", 0) or 0)
            wrap_u = int(getattr(settings, "m_WrapU", 1) or 1)
            texture = {
                "id": texture_name,
                "png": f"textures/{texture_name}.png",
                "source_bundle": source_bundle,
                "source_cab": texture_object_reader.assets_file.name,
                "source_path_id": path_id,
                "source_name": getattr(texture_object, "m_Name", texture_name),
                "width": int(texture_object.m_Width),
                "height": int(texture_object.m_Height),
                "format": TextureFormat(int(texture_object.m_TextureFormat)).name,
                "filter": filter_name(filter_mode),
                "wrap": wrap_name(wrap_u),
                "rgba8_sha256": hashlib.sha256(rgba).hexdigest(),
                "png_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
                "conversion": "UnityPy Texture2D.image fallback (not in conversion manifest)",
            }
            self.textures[texture_name] = texture
            return texture

        png_source = ROOT / entry["output_png"]
        if not png_source.exists():
            raise SystemExit(f"converted texture PNG missing: {png_source}")
        texture_object = texture_object_reader.read()

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
        if not (-4.0 <= pivot[0] <= 4.0 and -4.0 <= pivot[1] <= 4.0):
            raise SystemExit(
                f"sprite {sprite.m_Name} has an out-of-range adjusted pivot {pivot}"
            )
        # Tight-crop transparent padding can move the adjusted pivot slightly
        # beyond the texture rect. Clamp it to the renderable quad instead of
        # rejecting otherwise valid character frames.
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
            # Character-skin atlases are scene-specific and outside the fixed
            # conversion profile. Derive a stable name and bundle from the
            # sprite's texture PPtr, then use the direct UnityPy fallback.
            texture_reader = sprite.m_RD.texture
            cab = texture_reader.assetsfile.name
            source_bundle = self.cab_to_bundle.get(cab, "")
            texture_name = f"auto_atlas_{texture_path_id.replace('-', 'm')}"
            self.texture_info(texture_name, source_bundle, texture_path_id)
        else:
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

    @staticmethod
    def _curve_index(bindings, predicate, component=0):
        index = 0
        for binding in bindings:
            if predicate(binding):
                return index + component
            index += SceneBuilder._binding_curve_count(binding)
        return None

    @staticmethod
    def _streamed_curve(frames, curve_index, fallback):
        if not frames:
            return []
        initial = dict(frames[0][1])
        value = float(initial.get(curve_index, fallback))
        keys = [(0.0, value)]
        for time, entries in frames[1:]:
            if not math.isfinite(time) or time < 0.0:
                continue
            for index, entry_value in entries:
                if index != curve_index:
                    continue
                entry = float(entry_value)
                if time <= keys[-1][0] + 1e-7:
                    keys[-1] = (keys[-1][0], entry)
                else:
                    keys.append((float(time), entry))
        return [[time, value] for time, value in keys]

    def _door_animation_clip(self, clip, state_loop):
        bindings = list(clip.m_ClipBindingConstant.genericBindings)
        path_names = {
            zlib.crc32(name.encode("utf-8")) & 0xFFFFFFFF: name
            for name in
            [""] + [f"d{i}" for i in range(1, 6)] +
            [f"d{i}/d" for i in range(1, 6)] +
            [f"d{i}/shadow" for i in range(1, 6)]
        }

        def path_of(binding):
            return path_names.get(int(binding.path), str(binding.path))

        def is_type_attr(type_id, attribute, suffix=None, exact=None):
            def predicate(binding):
                if int(getattr(binding, "typeID", 0) or 0) != type_id:
                    return False
                if int(binding.attribute) != attribute:
                    return False
                path = path_of(binding)
                if suffix is not None and not path.endswith(suffix):
                    return False
                if exact is not None and path != exact:
                    return False
                return True
            return predicate

        frames = self._streamed_frames(clip)
        if not frames:
            raise SystemExit("door animation clip has no streamed frames")

        def curve(type_id, attribute, suffix=None, exact=None, component=0):
            index = self._curve_index(
                bindings, is_type_attr(type_id, attribute, suffix, exact),
                component)
            if index is None:
                return []
            return self._streamed_curve(frames, index, 0.0)

        leaf_extent = curve(4, 1, suffix="/d", component=1)
        shadow_visible = curve(212, 3305885265, suffix="/shadow")
        ornament_visible = curve(212, 3305885265, suffix="/d")
        collider_trigger = curve(61, 2693561734)
        collider_size_x = curve(61, 4197328169)
        collider_size_y = curve(61, 2368279999)

        sprite_curve_index = self._curve_index(
            bindings,
            lambda binding: (
                int(getattr(binding, "typeID", 0) or 0) == 212 and
                int(getattr(binding, "customType", 0)) == 23 and
                int(getattr(binding, "isPPtrCurve", 0)) == 1 and
                path_of(binding).startswith("d") and
                "/" not in path_of(binding)))
        leaf_sprites = []
        if sprite_curve_index is not None:
            mapping = list(clip.m_ClipBindingConstant.pptrCurveMapping)
            if not mapping:
                raise SystemExit("door animation clip has no sprite mapping")
            mapping_ids = [
                self.ensure_sprite_asset(mapping_ptr)[0]
                for mapping_ptr in mapping
            ]
            index_keys = self._streamed_curve(frames, sprite_curve_index, 0.0)
            for time, value in index_keys:
                sprite_index = int(round(value))
                if not (0 <= sprite_index < len(mapping_ids)):
                    raise SystemExit(
                        f"door sprite curve value {value} does not index "
                        f"pptrCurveMapping")
                leaf_sprites.append([time, mapping_ids[sprite_index]])

        duration = float(
            clip.m_MuscleClip.m_StopTime - clip.m_MuscleClip.m_StartTime)
        return {
            "id": getattr(clip, "m_Name", ""),
            "duration": duration,
            "sample_rate": float(clip.m_SampleRate),
            "loop": bool(state_loop) and bool(clip.m_MuscleClip.m_LoopTime),
            "leaf_extent": leaf_extent,
            "shadow_visible": shadow_visible,
            "ornament_visible": ornament_visible,
            "collider_trigger": collider_trigger,
            "collider_size_x": collider_size_x,
            "collider_size_y": collider_size_y,
            "leaf_sprites": leaf_sprites,
            "source": {
                "clip_path_id": str(clip.path_id if hasattr(clip, "path_id")
                                    else ""),
                "clip_name": getattr(clip, "m_Name", ""),
                "clip_start_time": float(clip.m_MuscleClip.m_StartTime),
                "clip_stop_time": float(clip.m_MuscleClip.m_StopTime),
                "sample_rate": float(clip.m_SampleRate),
                "loop_time": bool(clip.m_MuscleClip.m_LoopTime),
                "binding_count": len(bindings),
                "streamed_frame_count": len(frames),
            },
        }

    def _extract_door_animation(self, controller, controller_ptr):
        clips = {}
        for clip_ptr in controller.m_AnimationClips:
            clip = clip_ptr.read()
            clips[getattr(clip, "m_Name", "")] = (clip_ptr, clip)
        open_entry = clips.get("door_open")
        close_entry = clips.get("door_close") or clips.get("door_close2")
        if open_entry is None or close_entry is None:
            raise SystemExit(
                f"door controller {getattr(controller, 'm_Name', '')} is missing "
                f"door_open/door_close clips")
        state_machine = controller.m_Controller.m_StateMachineArray[0].data
        states = {}
        for state_offset in state_machine.m_StateConstantArray:
            state = state_offset.data
            states[int(state.m_NameID)] = state
        # Recovered state name hashes: door_open=3928941173,
        # door_close/door_close2=3801824743.
        open_state = states.get(3928941173)
        close_state = states.get(3801824743)
        return {
            "open": self._door_animation_clip(
                open_entry[1], bool(open_state.m_Loop) if open_state else False),
            "close": self._door_animation_clip(
                close_entry[1], bool(close_state.m_Loop) if close_state else False),
            "source": {
                "controller_path_id": str(controller_ptr.path_id),
                "controller_name": getattr(controller, "m_Name", ""),
                "open_clip_path_id": str(open_entry[0].path_id),
                "close_clip_path_id": str(close_entry[0].path_id),
            },
        }

    def _player_body_sprite(self, player_root):
        def find(transform):
            game_object = transform.m_GameObject.read()
            if getattr(game_object, "m_Name", "") == "body":
                for component in game_object.m_Component:
                    if component.component.type.name == "SpriteRenderer":
                        renderer = component.component.read()
                        if renderer.m_Sprite is not None and renderer.m_Sprite.path_id != 0:
                            return renderer.m_Sprite
            for child in transform.m_Children:
                found = find(child.read())
                if found is not None:
                    return found
            return None

        transform = next(
            component.component.read() for component in player_root.read().m_Component
            if component.component.type.name == "Transform")
        return find(transform)

    def _clip_sprite_keys(self, clip, binding_path: str):
        bindings = list(clip.m_ClipBindingConstant.genericBindings)
        target_hash = zlib.crc32(binding_path.encode("utf-8")) & 0xFFFFFFFF
        target_index = None
        for index, binding in enumerate(bindings):
            if (int(binding.path) == target_hash and
                    int(binding.customType) == 23 and
                    int(getattr(binding, "isPPtrCurve", 0)) == 1 and
                    int(getattr(binding, "typeID", 0) or 0) == 212):
                target_index = index
                break
        if target_index is None:
            return []
        curve_index = sum(
            self._binding_curve_count(b) for b in bindings[:target_index])
        frames = self._streamed_frames(clip)
        if not frames:
            return []
        mapping = list(clip.m_ClipBindingConstant.pptrCurveMapping)
        if not mapping:
            return []
        mapping_ids = []
        for mapping_ptr in mapping:
            if mapping_ptr is None or mapping_ptr.path_id == 0:
                mapping_ids.append(None)
            else:
                mapping_ids.append(self.ensure_sprite_asset(mapping_ptr)[0])
        initial = dict(frames[0][1])
        index_keys = [(0.0, float(initial.get(curve_index, 0.0)))]
        for time, entries in frames[1:]:
            if not math.isfinite(time) or time < 0.0:
                continue
            for index, value in entries:
                if index != curve_index:
                    continue
                value = float(value)
                if time <= index_keys[-1][0] + 1e-7:
                    index_keys[-1] = (index_keys[-1][0], value)
                else:
                    index_keys.append((float(time), value))
        keys = []
        for time, value in index_keys:
            mapped = int(round(value))
            if not (0 <= mapped < len(mapping_ids)) or mapping_ids[mapped] is None:
                raise SystemExit(
                    f"clip {getattr(clip, 'm_Name', '')} sprite value {value} "
                    f"does not index a valid pptrCurveMapping sprite")
            keys.append({"time": time, "sprite": mapping_ids[mapped]})
        return keys

    def _character_dead_sprite(self, controller, state_machine):
        for state_offset in state_machine.m_StateConstantArray:
            state = state_offset.data
            if int(state.m_NameID) != 3978643748:
                continue
            clip_index = int(
                state.m_BlendTreeConstantArray[0].data.m_NodeArray[0].data.m_ClipID)
            clip = controller.m_AnimationClips[clip_index].read()
            keys = self._clip_sprite_keys(clip, PLAYER_BODY_PATH)
            if keys:
                return keys[-1]["sprite"]
        return None

    def _clip_weapon_visual_curves(self, clip, attack_time):
        bindings = getattr(clip.m_ClipBindingConstant, "genericBindings", []) or []
        expanded = []
        for binding_index, binding in enumerate(bindings):
            type_id = int(getattr(binding, "typeID", 0) or 0)
            attribute = int(getattr(binding, "attribute", 0) or 0)
            path = int(getattr(binding, "path", 0) or 0)
            components = 1
            if type_id == 4 and attribute in (1, 3, 4):
                components = 3
            elif type_id == 4 and attribute == 2:
                components = 4
            for component in range(components):
                expanded.append((binding_index, component, type_id,
                                 attribute, path))
        raw = getattr(clip.m_MuscleClip.m_Clip.data.m_StreamedClip, "data", []) or []
        data = b"".join(struct.pack("<I", int(value) & 0xFFFFFFFF) for value in raw)
        values = {}
        position = 0
        while position + 8 <= len(data):
            time = struct.unpack_from("<f", data, position)[0]
            key_count = struct.unpack_from("<I", data, position + 4)[0]
            position += 8
            if key_count > 512 or position + key_count * 20 > len(data):
                break
            if not math.isfinite(time) or time < 0.0:
                position += key_count * 20
                continue
            for _ in range(key_count):
                curve_index = struct.unpack_from("<I", data, position)[0]
                _, _, _, value = struct.unpack_from("<4f", data, position + 4)
                position += 20
                if curve_index < len(expanded):
                    binding_index, component, type_id, attribute, path = expanded[curve_index]
                    values.setdefault((binding_index, component), []).append(
                        (time, float(value)))
        path_w = zlib.crc32(b"w") & 0xFFFFFFFF
        path_gun = zlib.crc32(b"w/gun_point") & 0xFFFFFFFF
        position_x = []
        rotation_z = []
        muzzle = []
        for binding_index, component, type_id, attribute, path in expanded:
            keys = values.get((binding_index, component), [])
            if not keys:
                continue
            if path == path_w and type_id == 4 and attribute == 1 and component == 0:
                position_x = sorted(keys)
            elif path == path_w and type_id == 4 and attribute == 2 and component == 2:
                z_keys = dict(keys)
                w_keys = dict(values.get((binding_index, 3), []))
                merged = []
                for time in sorted(set(z_keys) | set(w_keys)):
                    z = z_keys.get(time, 0.0)
                    w = w_keys.get(time, 1.0)
                    merged.append((time, math.degrees(2.0 * math.atan2(z, w))))
                rotation_z = merged
            elif path == path_gun and type_id == 212 and attribute == 3305885265:
                muzzle = sorted(keys)
        return {
            "visual_position_x": [[t, v] for t, v in position_x],
            "visual_rotation_z": [[t, v] for t, v in rotation_z],
            "muzzle_enabled": [[t, v] for t, v in muzzle],
            "visual_attack_time": float(attack_time),
        }

    def _extract_pet(self, pet_id, pet_go):
        pet_data = pet_go.read()
        role = None
        controller = None
        animator = None
        for component in pet_data.m_Component:
            if component.component.type.name == "MonoBehaviour":
                try:
                    candidate = component.component.read()
                except Exception:
                    continue
                if hasattr(candidate, "max_hp") and hasattr(candidate, "speed"):
                    role = candidate
                elif hasattr(candidate, "atk_cd") and hasattr(candidate, "damage"):
                    controller = candidate
            elif component.component.type.name == "Animator":
                animator = component.component.read()
        if role is None or controller is None or animator is None or not animator.m_Controller:
            raise RuntimeError("missing RoleAttribute/RGPetController/Animator")
        pet_transform = next(c.component.read() for c in pet_data.m_Component
                             if c.component.type.name == "Transform")
        shadow_sprite = None
        def find_shadow(tf, depth=0):
            nonlocal shadow_sprite
            if depth > 4 or shadow_sprite is not None:
                return
            go = tf.m_GameObject.read()
            if go.m_Name == "shadow":
                for c in go.m_Component:
                    if c.component.type.name == "SpriteRenderer":
                        shadow_sprite, _ = self.ensure_sprite_asset(
                            c.component.read().m_Sprite)
                        return
            for child in getattr(tf, "m_Children", []) or []:
                find_shadow(child.read(), depth + 1)
        find_shadow(pet_transform)
        ctrl = animator.m_Controller.read()
        clips = [p.read() for p in getattr(ctrl, "m_AnimationClips", []) or []]
        def clip_with(suffixes):
            for clip in clips:
                name = str(getattr(clip, "m_Name", ""))
                if any(name.endswith(suffix) for suffix in suffixes):
                    return clip
            return None
        idle_clip = clip_with(("_ide", "_idle"))
        run_clip = clip_with(("_run",))
        atk_clip = clip_with(("_atk", "_attack", "char_atk"))
        dead_clip = clip_with(("_dead", "_death"))
        def frames(clip):
            if clip is None:
                return []
            for path in ("img/body", "body"):
                try:
                    keys = self._clip_sprite_keys(clip, path)
                except Exception:
                    keys = []
                if keys:
                    return keys
            return []
        idle_frames = frames(idle_clip)
        if not idle_frames:
            # Some pet controllers use one static body sprite instead of an
            # idle PPtr clip (pet_datura/battery). Use the serialized body.
            def find_body(tf, depth=0):
                if depth > 4:
                    return None
                go = tf.m_GameObject.read()
                if go.m_Name == "body":
                    for c in go.m_Component:
                        if c.component.type.name == "SpriteRenderer":
                            sprite_id, _ = self.ensure_sprite_asset(
                                c.component.read().m_Sprite)
                            return sprite_id
                for child in getattr(tf, "m_Children", []) or []:
                    found = find_body(child.read(), depth + 1)
                    if found:
                        return found
                return None
            static_body = find_body(pet_transform)
            if not static_body:
                raise RuntimeError("no idle body sprite mapping")
            idle_frames = [{"time": 0.0, "sprite": static_body}]
        run_frames = frames(run_clip) or idle_frames
        atk_frames = frames(atk_clip) or idle_frames
        dead_frames = frames(dead_clip) or idle_frames
        body_sprite = idle_frames[0]["sprite"]
        return {
            "id": pet_id,
            "prefab": pet_id,
            "class_name": type(controller).__name__,
            "max_hp": int(role.max_hp),
            "speed": float(role.speed),
            "speed_rate": float(getattr(role, "speed_rate", 0.0) or 0.0),
            "damage": int(getattr(controller, "damage", 0) or 0),
            "atk_cd": float(getattr(controller, "atk_cd", 2.0) or 2.0),
            "min_follow_distance": float(
                getattr(controller, "min_follow_distance", 2.0) or 2.0),
            "max_follow_distance": float(
                getattr(controller, "max_follow_distance", 20.0) or 20.0),
            "min_distance": float(getattr(controller, "min_distance", 8.0) or 8.0),
            "atk_range": float(getattr(controller, "atk_range", 2.0) or 2.0),
            "is_melee": bool(getattr(controller, "is_melee", True)),
            "body_sprite": body_sprite,
            "shadow_sprite": shadow_sprite,
            "idle_sprites": idle_frames,
            "run_sprites": run_frames,
            "attack_sprites": atk_frames,
            "dead_sprites": dead_frames,
            "classification": (
                f"CONFIRMED {pet_id}/{type(controller).__name__} serialized "
                "RoleAttribute and Animator; follow/attack fields from the "
                "recovered RGPetController family"),
        }

    def _extract_enemy(self, enemy_id, enemy_go):
        enemy_data = enemy_go.read()
        role = None
        animator = None
        controller = None
        for component in enemy_data.m_Component:
            if component.component.type.name == "MonoBehaviour":
                try: candidate = component.component.read()
                except Exception: continue
                if hasattr(candidate, "max_hp") and hasattr(candidate, "speed"):
                    role = candidate
                elif hasattr(candidate, "enemy_id"):
                    controller = candidate
            elif component.component.type.name == "Animator":
                animator = component.component.read()
        if role is None or animator is None or not animator.m_Controller:
            raise RuntimeError("missing RoleAttribute/Animator")
        ctrl = animator.m_Controller.read()
        clips = [p.read() for p in getattr(ctrl, "m_AnimationClips", []) or []]
        idle_clip = next((c for c in clips
                          if str(getattr(c, "m_Name", "")).endswith("_ide")), None)
        dead_clip = next((c for c in clips
                          if str(getattr(c, "m_Name", "")).endswith("_dead")), None)
        if idle_clip is None:
            raise RuntimeError("no idle clip")
        enemy_transform = next(c.component.read() for c in enemy_data.m_Component
                               if c.component.type.name == "Transform")
        idle_keys = None
        for path in ("img/body", "body", "img"):
            try:
                idle_keys = self._clip_sprite_keys(idle_clip, path)
            except (Exception, SystemExit):
                idle_keys = None
            if idle_keys: break
        if not idle_keys:
            # Some enemies use a static body SpriteRenderer instead of an idle
            # PPtr clip (battery/crystal/malphite families). Use the serialized
            # body sprite rather than dropping the prefab.
            def find_body(tf, depth=0):
                if depth > 4:
                    return None
                go = tf.m_GameObject.read()
                if go.m_Name == "body":
                    for c in go.m_Component:
                        if c.component.type.name == "SpriteRenderer":
                            sprite_id, _ = self.ensure_sprite_asset(
                                c.component.read().m_Sprite)
                            return sprite_id
                for child in getattr(tf, "m_Children", []) or []:
                    found = find_body(child.read(), depth + 1)
                    if found:
                        return found
                return None
            static_body = find_body(enemy_transform)
            if static_body is None:
                raise RuntimeError("no idle body sprite mapping")
            idle_keys = [{"time": 0.0, "sprite": static_body}]
        dead_keys = idle_keys
        if dead_clip is not None:
            for path in ("img/body", "body", "img"):
                try: dead_keys = self._clip_sprite_keys(dead_clip, path)
                except (Exception, SystemExit): dead_keys = None
                if dead_keys: break
            if not dead_keys: dead_keys = idle_keys
        body_sprite = idle_keys[0]["sprite"]
        dead_sprite = dead_keys[-1]["sprite"]
        shadow_sprite = None
        enemy_transform = next(c.component.read() for c in enemy_data.m_Component
                               if c.component.type.name == "Transform")
        def find_shadow(tf, depth=0):
            nonlocal shadow_sprite
            if depth > 3 or shadow_sprite is not None: return
            go = tf.m_GameObject.read()
            if go.m_Name == "shadow":
                for c in go.m_Component:
                    if c.component.type.name == "SpriteRenderer":
                        shadow_sprite, _ = self.ensure_sprite_asset(
                            c.component.read().m_Sprite)
                        return
            for child in getattr(tf, "m_Children", []) or []:
                find_shadow(child.read(), depth + 1)
        find_shadow(enemy_transform)
        collider = next((c.component.read() for c in enemy_data.m_Component
                         if c.component.type.name == "BoxCollider2D"), None)
        if collider is None:
            raise RuntimeError("no root BoxCollider2D")
        weapon_atk = 0
        weapon_speed = 0.0
        weapon_size = 1.0
        weapon_bullet_ptr = None
        def find_weapon(tf, depth=0):
            nonlocal weapon_atk, weapon_speed, weapon_size, weapon_bullet_ptr
            if depth > 4:
                return
            go = tf.m_GameObject.read()
            if go.m_Name == "weapon":
                for c in go.m_Component:
                    if c.component.type.name != "MonoBehaviour":
                        continue
                    try:
                        candidate = c.component.read()
                    except Exception:
                        continue
                    if hasattr(candidate, "atk") and hasattr(candidate, "bullet"):
                        weapon_atk = int(getattr(candidate, "atk", 0) or 0)
                        weapon_speed = float(getattr(candidate, "bullet_speed", 0.0) or 0.0)
                        weapon_size = float(getattr(candidate, "bulletSize", 1.0) or 1.0)
                        ptr = getattr(candidate, "bullet", None)
                        if ptr is not None and getattr(ptr, "path_id", 0) != 0:
                            weapon_bullet_ptr = ptr
                        return
            for child in getattr(tf, "m_Children", []) or []:
                find_weapon(child.read(), depth + 1)
        find_weapon(enemy_transform)
        def has_child_named(tf, name, depth=0):
            if depth > 5:
                return False
            for child in getattr(tf, "m_Children", []) or []:
                child_tf = child.read()
                child_go = child_tf.m_GameObject.read()
                if child_go.m_Name == name:
                    return True
                if has_child_named(child_tf, name, depth + 1):
                    return True
            return False
        has_h2 = has_child_named(enemy_transform, "h2")
        projectile = None
        projectile_speed = 0.0
        projectile_damage = 0
        ranged = False
        bullet_ptr = weapon_bullet_ptr
        if controller is not None:
            for attr in ("bullet01", "bullet", "missile"):
                ptr = getattr(controller, attr, None)
                if ptr is not None and getattr(ptr, "path_id", 0) != 0:
                    bullet_ptr = ptr
                    break
        if bullet_ptr is not None:
            try:
                bullet_go = bullet_ptr.read()
                bullet_tf = next(c.component.read() for c in bullet_go.m_Component
                                 if c.component.type.name == "Transform")
                bullet_renderer_go = bullet_go
                for child in getattr(bullet_tf, "m_Children", []) or []:
                    ch = child.read()
                    go = ch.m_GameObject.read()
                    if go.m_Name == "b":
                        bullet_renderer_go = go
                        break
                bullet_sr = next((c.component.read() for c in bullet_renderer_go.m_Component
                                  if c.component.type.name == "SpriteRenderer"), None)
                bullet_col = next((c.component.read() for c in bullet_renderer_go.m_Component
                                   if c.component.type.name == "BoxCollider2D"), None)
                bullet_script = next((c.component.read() for c in bullet_go.m_Component
                                      if c.component.type.name == "MonoBehaviour" and
                                      hasattr(c.component.read(), "destroy_time")), None)
                if bullet_sr is not None:
                    sprite_id, _ = self.ensure_sprite_asset(bullet_sr.m_Sprite)
                    size = [0.5, 0.4]
                    offset = [0.0, 0.0]
                    if bullet_col is not None:
                        size = [float(bullet_col.m_Size.x), float(bullet_col.m_Size.y)]
                        offset = [float(bullet_col.m_Offset.x), float(bullet_col.m_Offset.y)]
                    projectile = {
                        "id": str(getattr(bullet_go, "m_Name", "enemy_bullet")),
                        "sprite": sprite_id,
                        "prefab_speed": float(getattr(bullet_script, "speed", 10.0)),
                        "lifetime": float(getattr(bullet_script, "destroy_time", 4.0)),
                        "camp": 0,
                        "can_rebound": bool(getattr(bullet_script, "can_rebound", False)),
                        "collider_size": size,
                        "collider_offset": offset,
                        "sorting_layer": int(bullet_sr.m_SortingLayer),
                        "sorting_order": int(bullet_sr.m_SortingOrder),
                    }
                    projectile_speed = max(
                        weapon_speed,
                        float(getattr(controller, "bullet_speed", 0.0) or
                              projectile["prefab_speed"]))
                    projectile_damage = max(
                        weapon_atk,
                        int(getattr(controller, "damage", 0) or
                            getattr(bullet_script, "damage", 0) or 0))
                    ranged = True
            except Exception:
                projectile = None
        controller_class = ""
        if controller is not None:
            try:
                script = controller.m_Script.read()
                controller_class = str(getattr(script, "m_ClassName", ""))
            except Exception:
                controller_class = ""
        projectile_count = 1
        fan_angle = 0.0
        burst_count = 1
        burst_interval = 0.0
        dash_speed = 0.0
        dash_duration = 0.0
        death_radius = 0.0
        death_damage = 0
        stationary = False
        mimic = False
        if controller_class == "EnemyAI01" and has_h2:
            projectile_count = 2
        elif controller_class == "EnemyAI20":
            bullet_count = int(getattr(controller, "bullet_count", 0) or 0)
            projectile_count = 2 * bullet_count + 1
            fan_angle = float(getattr(controller, "angle", 0.0) or 0.0)
        elif controller_class in ("EnemyAI11",):
            projectile_count = 2
            fan_angle = 30.0
            burst_count = 5
            burst_interval = 0.15
        elif controller_class == "EnemyAI14":
            burst_count = 32
            burst_interval = 0.05
        elif controller_class == "EnemyAI10":
            secondary = getattr(controller, "bullet02", None)
            projectile_count = 2 if secondary is not None and getattr(
                secondary, "path_id", 0) != 0 else 1
        elif controller_class in ("EnemyAI03", "EnemyAI18", "EnemyAI19"):
            shoot_time = float(getattr(controller, "shoot_time", 0.0) or 0.0)
            burst_count = max(1, int(shoot_time / 0.2) + 1) if shoot_time > 0.0 else 1
            burst_interval = 0.2
        elif controller_class in ("EnemyAI09", "EnemyAIFrog",
                                  "EnemyAIWitch"):
            dash_speed = 10.0 if controller_class == "EnemyAIWitch" else 8.0
            dash_duration = 1.5 if controller_class == "EnemyAIWitch" else (
                0.5 if controller_class == "EnemyAIFrog" else 1.5)
            if controller_class == "EnemyAIWitch":
                projectile_count = 2
                burst_count = 7
                burst_interval = 0.225
            if controller_class == "EnemyAIFrog":
                death_radius = 2.5
                death_damage = 10
        elif controller_class == "EnemyAI13":
            death_radius = 6.0
            death_damage = 10
        elif controller_class == "EnemyAI06":
            stationary = True
        elif controller_class == "EnemyAI18":
            stationary = True
        elif controller_class == "EnemyAI08":
            mimic = True
        return {
            "id": enemy_id,
            "path_id": str(enemy_go.path_id),
            "class_name": controller_class,
            "projectile_count": projectile_count,
            "fan_angle": fan_angle,
            "burst_count": burst_count,
            "burst_interval": burst_interval,
            "dash_speed": dash_speed,
            "dash_duration": dash_duration,
            "death_explosion_radius": death_radius,
            "death_explosion_damage": death_damage,
            "stationary": stationary,
            "mimic": mimic,
            "stand_in_shooting": bool(
                getattr(controller, "stand_in_shooting", False)),
            "lock_in_shooting": bool(
                getattr(controller, "lock_in_shooting", False)),
            "shoot_time": float(getattr(controller, "shoot_time", 0.0) or 0.0),
            "body_sprite": body_sprite,
            "dead_sprite": dead_sprite,
            "shadow_sprite": shadow_sprite,
            "hp": int(role.max_hp),
            "speed": int(role.speed),
            "camp": int(role.camp),
            "contact_damage": max(weapon_atk,
                int(getattr(controller, "damage", 0) or 0)),
            "contact_cooldown": float(getattr(controller, "later_time", 1.5) or 1.5),
            "collider_size": [float(collider.m_Size.x), float(collider.m_Size.y)],
            "collider_offset": [float(collider.m_Offset.x), float(collider.m_Offset.y)],
            "death_despawn_delay": 4.0,
            "ranged": ranged,
            "projectile": projectile or {},
            "projectile_speed": projectile_speed,
            "projectile_damage": projectile_damage,
            "shoot_cooldown": float(getattr(controller, "shoot_cd", 2.0) or 2.0),
            "attack_range": float(getattr(controller, "atk_range", 8.0) or 8.0),
            "body_sorting_layer": 5,
            "body_sorting_order": 1,
            "shadow_sorting_layer": 3,
            "shadow_sorting_order": 0,
            "idle_sprites": idle_keys,
            "classification": (
                f"CONFIRMED {enemy_id} RoleAttribute/Animator; "
                f"{getattr(controller, 'm_Script', None) is not None and 'controller' or 'no controller'}; "
                f"generic chase/contact and optional ranged bullet fields"),
        }

    def _combat_metadata(self):
        # Discover every serialized weapon prefab. The extraction below only
        # accepts prefabs with a real visual, bullet prefab and Attack
        # AnimationEvent; failures are classified instead of substituted.
        weapon_objects = {}
        for obj in self.env.objects:
            if obj.type.name != "GameObject":
                continue
            try:
                name = str(obj.read().m_Name)
            except Exception:
                continue
            if re.fullmatch(r"weapon_(?:\d{3}|init_[a-z0-9_]+)", name):
                weapon_objects.setdefault(name, obj)
        def weapon_sort_key(value):
            suffix = value.split("_", 1)[1]
            return (0, int(suffix), "") if suffix.isdigit() else (1, 0, suffix)
        weapon_ids = sorted(weapon_objects, key=weapon_sort_key)
        # c00 RoleAttributePlayer: the playable dev-room combat state.
        for obj in self.env.objects:
            if obj.type.name != "GameObject":
                continue
            game_object = obj.read()
            if getattr(game_object, "m_Name", "") != "c00":
                continue
            for component in game_object.m_Component:
                if component.component.type.name != "MonoBehaviour":
                    continue
                candidate = component.component.read()
                if hasattr(candidate, "max_hp") and hasattr(candidate, "max_armor"):
                    self.player_stats = {
                        "character_id": "c00",
                        "max_hp": int(candidate.max_hp),
                        "hp": int(candidate.hp),
                        "max_armor": int(candidate.max_armor),
                        "armor": int(candidate.armor),
                        "max_energy": int(candidate.max_energy),
                        "energy": int(getattr(candidate, "energy", candidate.max_energy)),
                        "camp": int(candidate.camp),
                        "atk": int(candidate.atk),
                        "critical": int(candidate.critical),
                        "speed": float(candidate.speed),
                        "speed_rate": float(candidate.speed_rate),
                        "deviation": int(getattr(candidate, "deviation", 0) or 0),
                        "classification": (
                            "CONFIRMED serialized c00 RoleAttributePlayer; "
                            "deviation is not serialized and defaults to 0"),
                    }
                    break
            if self.player_stats:
                break
        if not self.player_stats:
            raise SystemExit("c00 RoleAttributePlayer stats not found")
        for weapon_id in weapon_ids:
            try:
                weapon_go = weapon_objects[weapon_id]
                weapon_data = weapon_go.read()
                gun = None
                gun_class = ""
                weapon_animator = None
                weapon_transform = None
                for component in weapon_data.m_Component:
                    component_type = component.component.type.name
                    if component_type == "MonoBehaviour":
                        candidate = component.component.read()
                        if hasattr(candidate, "bulletsInfo"):
                            gun = candidate
                            try:
                                script = candidate.m_Script.read()
                                gun_class = str(getattr(script, "m_ClassName", ""))
                            except Exception:
                                gun_class = ""
                    elif component_type == "Animator":
                        weapon_animator = component.component.read()
                    elif component_type == "Transform":
                        weapon_transform = component.component.read()
                if gun is None or weapon_animator is None or weapon_transform is None:
                    raise SystemExit(f"{weapon_id} lacks Gun001/Animator/Transform")
                is_melee_serialized = bool(getattr(gun, "is_melee", False))

                def mono_components(go):
                    result = []
                    if go is None or getattr(go, "m_Component", None) is None:
                        return result
                    for component in go.m_Component:
                        if component.component.type.name != "MonoBehaviour":
                            continue
                        try:
                            result.append(component.component.read())
                        except Exception:
                            continue
                    return result

                def field_value(go, *names):
                    for mono in mono_components(go):
                        for name in names:
                            if hasattr(mono, name):
                                return getattr(mono, name)
                    return None

                def class_name(go):
                    for mono in mono_components(go):
                        try:
                            script = mono.m_Script.read()
                            name = str(getattr(script, "m_ClassName", ""))
                            if name and name != "MonoBehaviour":
                                return name
                        except Exception:
                            continue
                    return ""

                def clip_name(pointer):
                    try:
                        if pointer is None or getattr(pointer, "path_id", 0) == 0:
                            return ""
                        return str(getattr(pointer.read(), "m_Name", ""))
                    except Exception:
                        return ""

                def sprite_id_of(pointer):
                    try:
                        if pointer is None or getattr(pointer, "path_id", 0) == 0:
                            return None
                        return self.ensure_sprite_asset(pointer)[0]
                    except Exception:
                        return None

                def class_field(go, cls, *names):
                    for mono in mono_components(go):
                        try:
                            script = mono.m_Script.read()
                            mono_class = str(getattr(script, "m_ClassName", ""))
                        except Exception:
                            continue
                        if mono_class != cls:
                            continue
                        for name in names:
                            if hasattr(mono, name):
                                return getattr(mono, name)
                    return None

                def find_sprite_renderer(go, depth=0):
                    if go is None or depth > 6:
                        return None
                    sr = next((c.component.read() for c in go.m_Component
                               if c.component.type.name == "SpriteRenderer"),
                              None)
                    if sr is not None and sr.m_Sprite and sr.m_Sprite.path_id:
                        return sr
                    transform = next((c.component.read() for c in go.m_Component
                                      if c.component.type.name == "Transform"),
                                     None)
                    if transform is None:
                        return None
                    for child in getattr(transform, "m_Children", []) or []:
                        found = find_sprite_renderer(
                            child.read().m_GameObject.read(), depth + 1)
                        if found is not None:
                            return found
                    return None

                def first_sprite_id(go):
                    sr = find_sprite_renderer(go)
                    if sr is None:
                        return None, None
                    return self.ensure_sprite_asset(sr.m_Sprite)

                def find_child(go, name, depth=0):
                    if go is None or depth > 6:
                        return None
                    transform = next((c.component.read() for c in go.m_Component
                                      if c.component.type.name == "Transform"),
                                     None)
                    if transform is None:
                        return None
                    for child in getattr(transform, "m_Children", []) or []:
                        child_go = child.read().m_GameObject.read()
                        if child_go.m_Name == name:
                            return child_go
                        found = find_child(child_go, name, depth + 1)
                        if found is not None:
                            return found
                    return None

                def deref(pointer):
                    try:
                        if pointer is None or getattr(pointer, "path_id", 0) == 0:
                            return None
                        return pointer.read()
                    except Exception:
                        return None

                def bullet_info_for(info):
                    if info is None:
                        return None, None
                    go = deref(info.bulletProto)
                    return info, go

                bullet_infos = list(getattr(gun, "bulletsInfo", []) or [])
                bullet_info, bullet_go = bullet_info_for(
                    bullet_infos[0] if bullet_infos else None)
                has_bullet = (not is_melee_serialized and
                              bullet_info is not None and bullet_go is not None)
                bullet_data = bullet_go
                bullet_sr = find_sprite_renderer(bullet_go) if has_bullet else None
                collider_size = [0.5, 0.4]
                collider_offset = [0.25, 0.0]
                bullet_script = None
                if bullet_data is not None:
                    bullet_collider = next(
                        (c.component.read() for c in bullet_data.m_Component
                         if c.component.type.name == "BoxCollider2D"), None)
                    bullet_script = next(
                        (c.component.read() for c in bullet_data.m_Component
                         if c.component.type.name == "MonoBehaviour" and
                         hasattr(c.component.read(), "destroy_time")), None)
                    if bullet_collider is not None:
                        collider_size = [float(bullet_collider.m_Size.x),
                                         float(bullet_collider.m_Size.y)]
                        collider_offset = [float(bullet_collider.m_Offset.x),
                                           float(bullet_collider.m_Offset.y)]
                bullet_sprite = (self.ensure_sprite_asset(bullet_sr.m_Sprite)[0]
                                 if bullet_sr is not None else None)

                # Read the weapon visual (`w` child) and gun_point transform.
                visual_sprite = None
                visual_offset = [0.0, 0.0]
                muzzle_offset = [0.6, 0.15]
                muzzle_sprite = None
                visual_rotation = 0.0
                visual_layer = 5
                visual_order = 1
                for child in weapon_transform.m_Children:
                    transform = child.read()
                    game_object = transform.m_GameObject.read()
                    if game_object.m_Name == "w":
                        visual_offset = [float(transform.m_LocalPosition.x),
                                         float(transform.m_LocalPosition.y)]
                        visual_rotation = float(transform.m_LocalRotation.z)
                        sr = next(
                            (c.component.read() for c in game_object.m_Component
                             if c.component.type.name == "SpriteRenderer"), None)
                        if sr is not None:
                            visual_sprite, _ = self.ensure_sprite_asset(sr.m_Sprite)
                            visual_layer = int(sr.m_SortingLayer)
                            visual_order = int(sr.m_SortingOrder)
                        for w_child in getattr(transform, "m_Children", []) or []:
                            w_transform = w_child.read()
                            w_go = w_transform.m_GameObject.read()
                            if w_go.m_Name != "gun_point":
                                continue
                            muzzle_offset = [
                                float(w_transform.m_LocalPosition.x),
                                float(w_transform.m_LocalPosition.y)]
                            muzzle_sr = next(
                                (c.component.read() for c in w_go.m_Component
                                 if c.component.type.name == "SpriteRenderer"),
                                None)
                            if muzzle_sr is not None and muzzle_sr.m_Sprite:
                                muzzle_sprite, _ = self.ensure_sprite_asset(
                                    muzzle_sr.m_Sprite)
                    elif game_object.m_Name == "gun_point":
                        muzzle_offset = [float(transform.m_LocalPosition.x),
                                         float(transform.m_LocalPosition.y)]
                if visual_sprite is None:
                    raise SystemExit(f"{weapon_id} has no recoverable weapon visual")

                # Select the first non-idle state with an Attack event. Exact
                # event times drive multi-shot bursts; the full event list is
                # preserved rather than collapsing to the first shot.
                attack_cycle = 0.0
                first_shot_delay = 0.0
                attack_events = []
                attack_clip_name = ""
                attack_clip = None
                fallback_clip = None
                attack_controller = weapon_animator.m_Controller.read()
                override_clips = {}
                if hasattr(attack_controller, "m_Clips"):
                    for override in attack_controller.m_Clips:
                        try:
                            original_id = override.m_OriginalClip.path_id
                            replacement = override.m_OverrideClip.read()
                            if original_id and replacement is not None:
                                override_clips[original_id] = replacement
                        except Exception:
                            continue
                # AnimatorOverrideController stores a PPtr to its base
                # AnimatorController. Several recovered boss/unique weapons
                # (Extra Crown, Bayonet Rifle, Assault Sniper Rifle, New Year
                # Gift, Revolver) use override controllers; the state machine
                # and clips live on the base controller while m_Overrides
                # maps individual clips. The base names are the recovered
                # animation states for these prefabs.
                override_inner = getattr(attack_controller, "m_Controller", None)
                if override_inner is not None and type(override_inner).__name__ == "PPtr":
                    base_controller = override_inner.read()
                    if base_controller is not None:
                        attack_controller = base_controller
                attack_states = (
                    attack_controller.m_Controller.m_StateMachineArray[0].data
                    .m_StateConstantArray)
                for state_offset in attack_states:
                    state = state_offset.data
                    if (len(state.m_BlendTreeConstantArray) == 0 or
                            len(state.m_BlendTreeConstantArray[0].data
                                .m_NodeArray) == 0):
                        continue
                    clip_pointer = attack_controller.m_AnimationClips[
                        state.m_BlendTreeConstantArray[0].data.m_NodeArray[0]
                        .data.m_ClipID]
                    clip = override_clips.get(
                        getattr(clip_pointer, "path_id", 0)) or clip_pointer.read()
                    if getattr(clip, "m_Name", "") == "w_ide":
                        continue
                    speed_scale = (float(gun.weapon_speed)
                                   if float(gun.weapon_speed) > 0.0 else 1.0)
                    events = [e for e in clip.m_Events
                              if getattr(e, "functionName", "") == "Attack"]
                    if events:
                        attack_cycle = (float(clip.m_MuscleClip.m_StopTime) /
                                        speed_scale)
                        first_shot_delay = float(events[0].time) / speed_scale
                        attack_events = sorted(
                            float(e.time) / speed_scale for e in events)
                        attack_clip_name = getattr(clip, "m_Name", "")
                        attack_clip = clip
                        break
                    if (fallback_clip is None or
                            float(fallback_clip.m_MuscleClip.m_StopTime) <= 0.0):
                        fallback_clip = clip
                if (attack_cycle <= 0.0 and
                        (fallback_clip is None or
                         float(fallback_clip.m_MuscleClip.m_StopTime) <= 0.0)):
                    for clip_ptr in attack_controller.m_AnimationClips:
                        candidate_clip = override_clips.get(
                            getattr(clip_ptr, "path_id", 0)) or clip_ptr.read()
                        name = getattr(candidate_clip, "m_Name", "")
                        if name == "w_ide" or name.startswith("hide"):
                            continue
                        if float(candidate_clip.m_MuscleClip.m_StopTime) > 0.0:
                            fallback_clip = candidate_clip
                            break
                if attack_cycle <= 0.0 and fallback_clip is not None:
                    # Bows and a few weapons have no AnimationEvent; the
                    # serialized attack clip duration is still the cadence.
                    speed_scale = (float(gun.weapon_speed)
                                   if float(gun.weapon_speed) > 0.0 else 1.0)
                    attack_cycle = (float(fallback_clip.m_MuscleClip.m_StopTime) /
                                    speed_scale)
                    first_shot_delay = 0.0
                    attack_clip_name = getattr(fallback_clip, "m_Name", "")
                    attack_clip = fallback_clip
                if attack_cycle <= 0.0:
                    # Passive special weapons (Nirvana Cross, Trekking Pole)
                    # have no Attack event or usable attack state. They remain
                    # real definitions; runtime marks them passive.
                    if gun_class in ("GunRebornCross", "Gun012"):
                        attack_cycle = 1.0
                        first_shot_delay = 0.0
                        attack_clip_name = "passive"
                        attack_clip = None
                    else:
                        raise SystemExit(f"{weapon_id} has no usable attack clip")
                visual_curves = (self._clip_weapon_visual_curves(
                    attack_clip, first_shot_delay) if attack_clip is not None
                    else {"visual_position_x": [],
                          "visual_rotation_z": [],
                          "muzzle_enabled": []})

                catalogue_meta = WEAPON_CATALOGUE.get(weapon_id, {})

                def make_variant(info):
                    go = deref(info.bulletProto) if info is not None else None
                    sr = find_sprite_renderer(go)
                    sprite = (self.ensure_sprite_asset(sr.m_Sprite)[0]
                              if sr is not None else None)
                    script = None
                    if go is not None:
                        script = next(
                            (c.component.read() for c in go.m_Component
                             if c.component.type.name == "MonoBehaviour" and
                             hasattr(c.component.read(), "destroy_time")), None)
                    collider = None
                    if go is not None:
                        collider = next(
                            (c.component.read() for c in go.m_Component
                             if c.component.type.name == "BoxCollider2D"), None)
                    return {
                        "id": getattr(go, "m_Name", "melee"),
                        "sprite": sprite,
                        "prefab_speed": float(getattr(script, "speed", 10.0)),
                        "lifetime": float(getattr(script, "destroy_time", 5.0)),
                        "camp": int(getattr(script, "camp", 1)),
                        "can_rebound": bool(getattr(script, "can_rebound", False)),
                        "collider_size": ([float(collider.m_Size.x),
                                           float(collider.m_Size.y)]
                                          if collider is not None else [0.5, 0.4]),
                        "collider_offset": ([float(collider.m_Offset.x),
                                             float(collider.m_Offset.y)]
                                            if collider is not None else [0.25, 0.0]),
                        "sorting_layer": int(sr.m_SortingLayer) if sr else 6,
                        "sorting_order": int(sr.m_SortingOrder) if sr else 0,
                        "speed": float(getattr(info, "speed", 0.0)),
                        "damage": int(getattr(info, "damage", 0)),
                        "critic": int(getattr(info, "critic", 0)),
                        "repel": int(getattr(info, "repel", 0)),
                        "through": int(getattr(info, "throughCount", 0)),
                    }

                variants = [make_variant(info) for info in bullet_infos]
                # GunMultiBullet02 cycles its separate `bullets` array; the
                # bulletsInfo entry is only the shared speed/damage template.
                if gun_class == "GunMultiBullet02" and hasattr(gun, "bullets"):
                    cycle = []
                    for pointer in getattr(gun, "bullets", []) or []:
                        go = deref(pointer)
                        sr = find_sprite_renderer(go)
                        sprite = (self.ensure_sprite_asset(sr.m_Sprite)[0]
                                  if sr is not None else None)
                        script = next(
                            (c.component.read() for c in go.m_Component
                             if c.component.type.name == "MonoBehaviour" and
                             hasattr(c.component.read(), "destroy_time")), None)
                        collider = next(
                            (c.component.read() for c in go.m_Component
                             if c.component.type.name == "BoxCollider2D"), None)
                        cycle.append({
                            "id": getattr(go, "m_Name", "melee"),
                            "sprite": sprite,
                            "prefab_speed": float(getattr(script, "speed", 10.0)),
                            "lifetime": float(getattr(script, "destroy_time", 5.0)),
                            "camp": int(getattr(script, "camp", 1)),
                            "can_rebound": bool(getattr(script, "can_rebound", False)),
                            "collider_size": ([float(collider.m_Size.x),
                                               float(collider.m_Size.y)]
                                              if collider is not None else [0.5, 0.4]),
                            "collider_offset": ([float(collider.m_Offset.x),
                                                 float(collider.m_Offset.y)]
                                                if collider is not None else [0.25, 0.0]),
                            "sorting_layer": int(sr.m_SortingLayer) if sr else 6,
                            "sorting_order": int(sr.m_SortingOrder) if sr else 0,
                            "speed": float(getattr(bullet_info, "speed", 0.0)),
                            "damage": int(getattr(bullet_info, "damage", 0)),
                            "critic": int(getattr(bullet_info, "critic", 0)),
                            "repel": int(getattr(bullet_info, "repel", 0)),
                            "through": int(getattr(bullet_info, "throughCount", 0)),
                        })
                    if cycle:
                        variants = [dict(item) for item in cycle]
                        bullet_sprite = variants[0]["sprite"]

                special = {}
                special_kind = ""
                special_audio = ""
                special_events = []
                if gun_class == "GunSummon01":
                    npc_go = deref(getattr(gun, "bullet_2", None)) or bullet_go
                    body = find_child(npc_go, "body")
                    sprite = first_sprite_id(body or npc_go)[0]
                    special_kind = "summon_skeleton"
                    special = {
                        "kind": special_kind,
                        "count": 1,
                        "sprite": sprite,
                        "duration": float(class_field(
                            npc_go, "NpcSummon01", "life_time") or 15.0),
                        "hp": int(field_value(npc_go, "max_hp", "hp") or 8),
                        "damage": int(field_value(npc_go, "damage") or 2),
                        "speed": float(field_value(npc_go, "speed") or 6.0),
                        "interval": float(field_value(
                            npc_go, "atk_cd") or 2.0),
                        "range": float(field_value(
                            npc_go, "atk_range") or 2.0),
                    }
                elif gun_class == "GunPhantom":
                    npc_go = bullet_go
                    body = find_child(npc_go, "body")
                    sprite = first_sprite_id(body or npc_go)[0]
                    special_kind = "summon_phantom"
                    special = {
                        "kind": special_kind,
                        "count": max(1, int(getattr(gun, "count", 1) or 1)),
                        "sprite": sprite,
                        "duration": 20.0,
                        "hp": int(field_value(npc_go, "max_hp", "hp") or 100),
                        "damage": int(field_value(npc_go, "damage") or 1),
                        "speed": float(field_value(npc_go, "speed") or 6.0),
                        "interval": float(field_value(
                            npc_go, "atk_cd") or 1.0),
                        "range": float(field_value(
                            npc_go, "atk_range") or 2.0),
                        "value": float(class_field(
                            npc_go, "PhantomSplitProcessor",
                            "damageFactor") or 0.5),
                    }
                elif gun_class == "GunGourdWithFunnelSword":
                    funnel_go = deref(getattr(gun, "funnelSwords", [None])[0])
                    sword = find_child(funnel_go, "w") or funnel_go
                    sprite = first_sprite_id(sword)[0]
                    special_kind = "funnel_swords"
                    special = {
                        "kind": special_kind,
                        "count": max(1, int(getattr(
                            gun, "maxfunnelSwordCount", 2) or 2)),
                        "sprite": sprite,
                        "duration": float(getattr(gun, "funnelTime", 8) or 8),
                        "hp": int(field_value(funnel_go, "max_hp", "hp") or 5),
                        "damage": 1,
                        "speed": float(class_field(
                            funnel_go, "FunnelWeaponCurveContainer",
                            "speed") or 8.0),
                        "interval": float(class_field(
                            funnel_go, "FunnelWeaponCurveContainer",
                            "atk_cd") or 2.0),
                        "range": 2.0,
                    }
                elif gun_class == "GunCrown":
                    special_kind = "stealth"
                    special = {
                        "kind": special_kind,
                        "count": 1,
                        "duration": float(class_field(
                            bullet_go, "BuffStealth", "buff_time") or 10.0),
                        "value": float(getattr(getattr(
                            bullet_go, "buff_color", None), "a", 0.297)),
                    }
                elif gun_class == "GunChannel":
                    end_variant = (make_variant(bullet_infos[1])
                                   if len(bullet_infos) > 1 else None)
                    special_kind = "black_hole"
                    special = {
                        "kind": special_kind,
                        "count": 1,
                        "duration": float(getattr(
                            gun, "shoot_end_time", 0.8) or 0.8),
                        "interval": float(getattr(
                            gun, "max_mul_time", 1) or 1),
                        "damage": int(getattr(
                            bullet_infos[1], "damage", 0)
                            if len(bullet_infos) > 1 else 0),
                        "variants": ([end_variant] if end_variant else []),
                        "audio_clip": (
                            str(getattr(gun.clip_end_shoot.read(),
                                        "m_Name", ""))
                            if getattr(gun, "clip_end_shoot", None) is not None
                            and gun.clip_end_shoot.path_id != 0 else ""),
                    }
                elif gun_class == "GunCamera":
                    special_kind = "camera_flash"
                    special = {
                        "kind": special_kind,
                        "count": 1,
                        "duration": float(getattr(
                            gun, "lightTime", 0.15) or 0.15),
                        "range": float(field_value(
                            bullet_go, "range") or 13.0),
                        "damage": int(field_value(
                            bullet_go, "damage") or 2),
                        "interval": float(field_value(
                            bullet_go, "delay") or 0.1),
                        "value": float(field_value(
                            bullet_go, "repel") or 10.0),
                    }
                elif gun_class == "GunBox":
                    special_kind = "box_place"
                    special = {
                        "kind": special_kind,
                        "count": 1,
                        "sprite": bullet_sprite,
                        "hp": int(class_field(bullet_go, "RGBox", "hp") or 4),
                        "duration": float(getattr(gun, "cd", 0.0) or 0.0),
                        "value": float(getattr(
                            gun, "createDistance", 0.0) or 0.0),
                    }
                elif gun_class == "GunRebornCross":
                    special_kind = "revive"
                    special = {"kind": special_kind, "count": 1}
                elif gun_class == "GunTrump":
                    special_kind = "attack_speed"
                    special = {
                        "kind": special_kind,
                        "count": 1,
                        "duration": float(class_field(
                            bullet_go, "BuffAtkSpeedChange",
                            "buff_time") or 4.0),
                        "value": float(class_field(
                            bullet_go, "BuffAtkSpeedChange",
                            "change_val") or 25.0),
                        "damage": int(float(getattr(
                            gun, "add_atkspeed", 0.0) or 0.0)),
                    }
                elif gun_class == "GunDeadNote":
                    special_kind = "death_mark"
                    special = {
                        "kind": special_kind,
                        "count": 1,
                        "duration": float(class_field(
                            bullet_go, "BuffDeadNote", "buff_time") or 6.0),
                    }
                elif gun_class == "GunThrowSword":
                    special_kind = "bayonet"
                    special = {
                        "kind": special_kind,
                        "count": 2,
                        "value": float(getattr(
                            gun, "meleeDistance", 3.5) or 3.5),
                        "variants": variants[1:] if len(variants) > 1 else [],
                        "audio_clip": (
                            str(getattr(gun.throw_audio_clip.read(),
                                        "m_Name", ""))
                            if getattr(gun, "throw_audio_clip", None) is not None
                            and gun.throw_audio_clip.path_id != 0 else ""),
                    }
                elif gun_class == "Gun008":
                    count = max(1, int(getattr(
                        gun, "continuousCount", 1) or 1))
                    delay = float(getattr(gun, "delay", 0.0) or 0.0)
                    special_kind = "revolver_burst"
                    special_events = [first_shot_delay + i * delay
                                      for i in range(count)]
                    special = {
                        "kind": special_kind,
                        "count": count,
                        "interval": delay,
                        "variants": variants,
                    }
                elif gun_class == "Gun012":
                    count = max(0, int(getattr(
                        gun, "multiCount", 0) or 0))
                    if count > 0:
                        special_kind = "multi_pattern"
                        special = {
                            "kind": special_kind,
                            "count": count,
                            "value": float(getattr(
                                gun, "angle", 0.0) or 0.0),
                            "variants": variants,
                        }
                    else:
                        special_kind = "passive"
                        special = {"kind": special_kind, "count": 0}
                elif gun_class == "GunMultiBullet02":
                    special_kind = "multi_pattern"
                    special = {
                        "kind": special_kind,
                        "count": max(1, int(getattr(
                            gun, "multiCount", 1) or 1)),
                        "value": float(getattr(gun, "angle", 0.0) or 0.0),
                        "variants": variants,
                    }
                reload_go = deref(getattr(gun, "reload_obj", None))
                reload_sprite = (first_sprite_id(reload_go)[0]
                                 if reload_go is not None else None)
                if gun_class == "Gun020":
                    special_kind = "charge_gun020"
                    special = {
                        "kind": special_kind,
                        "duration": float(getattr(gun, "max_time", 2.0) or 2.0),
                        "hold_speed": float(getattr(gun, "holdSpeed", 1.0) or 1.0),
                        "a_size_max": float(getattr(gun, "a_size_max", 0.0) or 0.0),
                        "force": int(getattr(gun, "force", 0) or 0),
                        "reflect_count": int(getattr(gun, "reflectCount", 0) or 0),
                        "audio_clip": clip_name(getattr(gun, "clip_hold", None)),
                        "sprite": reload_sprite,
                        "audio_clip2": clip_name(getattr(gun, "clip_shoot_max", None)),
                        "variants": variants[1:] if len(variants) > 1 else [],
                    }
                elif gun_class == "Gun007":
                    special_kind = "charge_gun007"
                    special = {
                        "kind": special_kind,
                        "duration": float(getattr(gun, "max_time", 2.0) or 2.0),
                        "hold_speed": float(getattr(gun, "holdSpeed", 1.0) or 1.0),
                        "count": int(getattr(gun, "multiCount", 1) or 1),
                        "continuous_count": int(getattr(
                            gun, "continuousCount", 3) or 3),
                        "a_interval": float(getattr(gun, "a_interval", 0.0) or 0.0),
                        "angle": int(getattr(gun, "angle", 15) or 15),
                        "inactive_time": float(getattr(
                            gun, "inactiveTime", 0.1) or 0.1),
                        "audio_clip": clip_name(getattr(gun, "clip_hold", None)),
                        "sprite": reload_sprite,
                        "audio_clip2": clip_name(getattr(gun, "clip_big", None)),
                        "variants": variants[1:] if len(variants) > 1 else [],
                    }
                elif gun_class == "Gun005":
                    special_kind = "charge_bow"
                    special = {
                        "kind": special_kind,
                        "duration": float(getattr(gun, "max_time", 2.0) or 2.0),
                        "hold_speed": float(getattr(gun, "holdSpeed", 1.0) or 1.0),
                        "audio_clip": clip_name(getattr(gun, "clip_hold", None)),
                        "sprite": reload_sprite,
                        "sprite2": sprite_id_of(getattr(gun, "s_atk", None)),
                        "variants": variants[1:] if len(variants) > 1 else [],
                    }
                elif gun_class == "Gun011":
                    special_kind = "end_shot"
                    special = {
                        "kind": special_kind,
                        "duration": float(getattr(
                            gun, "shoot_end_time", 2.0) or 2.0),
                        "hold_speed": float(getattr(gun, "holdSpeed", 1.0) or 1.0),
                        "sprite": reload_sprite,
                        "audio_clip2": clip_name(getattr(gun, "clip_end_shoot", None)),
                        "variants": variants[1:] if len(variants) > 1 else [],
                    }
                if gun_class == "GunPot":
                    special_kind = "pot"
                    special = {
                        "kind": special_kind,
                        "hp": int(getattr(gun, "hp", 0) or 0),
                        "energy": int(getattr(gun, "energy", 0) or 0),
                        "audio_clip": clip_name(getattr(gun, "drinkClip", None)),
                        "sprite": sprite_id_of(getattr(gun, "emptySprite", None)),
                    }
                if (not special_kind and not is_melee_serialized and
                        bullet_sprite is None and bullet_info is not None):
                    # Sweet Talk's BulletMissle has a valid collider/damage but
                    # an intentionally invisible renderer (sprite path ID 0).
                    special_kind = "invisible_missile"
                    special = {"kind": special_kind, "count": 1}
                if gun_class == "GunThrow" and hasattr(gun, "prepare_time"):
                    special_kind = "eatable_throw"
                    special = {
                        "kind": special_kind,
                        "count": max(1, int(getattr(
                            gun, "multiCount", 1) or 1)),
                        "duration": float(getattr(
                            gun, "prepare_time", 0.0) or 0.0),
                        "value": float(getattr(
                            gun, "max_consume", 0.0) or 0.0),
                        "audio_clip": (
                            str(getattr(
                                getattr(gun, "WeaponSpecialEatable", None),
                                "animAudioClip", ""))
                            if False else ""),
                    }
                    eatable = next(
                        (mono for mono in mono_components(
                            weapon_data)
                         if hasattr(mono, "animAudioClip")), None)
                    if eatable is not None and eatable.animAudioClip is not None:
                        try:
                            special["audio_clip"] = str(
                                eatable.animAudioClip.read().m_Name)
                        except Exception:
                            pass
                if special_kind == "revolver_burst":
                    attack_events = special_events
                    first_shot_delay = attack_events[0] if attack_events else 0.0
                elif special_kind in ("passive", "revive"):
                    attack_events = []
                    first_shot_delay = 0.0
                if special and special.get("sprite") is None:
                    special.pop("sprite", None)

                projectile = {
                    "id": getattr(bullet_go, "m_Name", "melee"),
                    "sprite": bullet_sprite,
                    "prefab_speed": float(getattr(bullet_script, "speed", 10.0)),
                    "lifetime": float(getattr(bullet_script, "destroy_time", 5.0)),
                    "camp": int(getattr(bullet_script, "camp", 1)),
                    "can_rebound": bool(getattr(bullet_script, "can_rebound", False)),
                    "collider_size": collider_size,
                    "collider_offset": collider_offset,
                    "sorting_layer": int(bullet_sr.m_SortingLayer) if bullet_sr else 6,
                    "sorting_order": int(bullet_sr.m_SortingOrder) if bullet_sr else 0,
                    "classification": (
                        f"CONFIRMED {getattr(bullet_go, 'm_Name', 'melee')}/Bullet01 "
                        f"prefab; prefab speed overridden by weapon BulletInfo"
                        if has_bullet else "serialized special weapon without bullet"),
                }
                damage = max(
                    int(bullet_info.damage) if bullet_info is not None else 0,
                    int(catalogue_meta.get("damage", 0) or 0))
                if damage <= 0 and not special_kind and not is_melee_serialized:
                    print(f"SKIP {weapon_id}: non-positive serialized damage")
                    continue
                if not has_bullet and not is_melee_serialized and not special_kind:
                    print(f"SKIP {weapon_id}: no bullet and not melee")
                    continue
                if special_kind in ("multi_pattern", "revolver_burst"):
                    # These spawn their recovered projectile array directly.
                    pass
                elif special_kind and special_kind not in ("invisible_missile",):
                    # Subclass effects are emitted as gameplay events.
                    pass
                self.weapons.append({
                    "id": weapon_id,
                    "display_name": (
                        catalogue_meta.get("display_name")
                        or LOADOUT_WEAPON_NAMES.get(weapon_id)
                        or WEAPON_DISPLAY_NAMES.get(weapon_id, weapon_id)),
                    "class_name": gun_class,
                    "rarity": catalogue_meta.get("level"),
                    "drop_level": catalogue_meta.get("drop_level"),
                    "weapon_type_name": catalogue_meta.get("type"),
                    "forgeable": catalogue_meta.get("forgeable"),
                    "item_value": int(catalogue_meta.get("item_value", 0) or 0),
                    "projectile": projectile,
                    "bullet_speed": float(bullet_info.speed) if has_bullet else 0.0,
                    "size": float(bullet_info.size) if has_bullet else 1.0,
                    "damage": damage,
                    "critic": int(bullet_info.critic) if has_bullet else 0,
                    "deviation": int(gun.deviation),
                    "multi_count": int(getattr(gun, "multiCount", 0) or 0),
                    "spread_angle": int(getattr(gun, "angle", 0) or 0),
                    "is_melee": bool(getattr(gun, "is_melee", False)),
                    "melee_range": float(getattr(gun, "meleeDistance", 0.0) or
                        getattr(gun, "swordScale", 0.0) or
                        getattr(gun, "atk_range", 0.0) or 1.8),
                    "melee_arc": 110.0 if bool(getattr(gun, "is_melee", False)) else 0.0,
                    "owner_deviation": 0,
                    "consume": int(gun.consume),
                    "audio_clip": (
                        str(getattr(gun.audio_clip.read(), "m_Name", ""))
                        if getattr(gun, "audio_clip", None) is not None and
                        gun.audio_clip.path_id != 0 else ""),
                    "weapon_speed": float(gun.weapon_speed),
                    "atk_move_speed": float(gun.atk_move_speed),
                    "repel": int(bullet_info.repel) if has_bullet else 0,
                    "through": int(bullet_info.throughCount) if has_bullet else 0,
                    "attack_cycle_seconds": attack_cycle,
                    "first_shot_delay_seconds": first_shot_delay,
                    "attack_event_times": attack_events,
                    **({"special": special} if special else {}),
                    **visual_curves,
                    "visual_sprite": visual_sprite,
                    "visual_offset": visual_offset,
                    "muzzle_offset": muzzle_offset,
                    "muzzle_sprite": muzzle_sprite,
                    "visual_rotation": visual_rotation,
                    "sorting_layer": visual_layer,
                    "sorting_order": visual_order,
                    "classification": (
                        f"CONFIRMED {weapon_id}/Gun001 serialized fields; "
                        f"first Attack state {attack_clip_name}; display name from "
                        f"WeaponInfo/localization; attack cycle scaled by "
                        f"weapon_speed"),
                })
            except (Exception, SystemExit) as extraction_error:
                print(f"SKIP {weapon_id}: {extraction_error}")
                continue

        # Generic enemy extraction. Every prefab with RoleAttribute + Animator
        # becomes a runtime definition; only the biome-1 pool is used for the
        # current r1 fixtures. Failures remain catalogue-only, never substituted.
        enemy_objects = {}
        for obj in self.env.objects:
            if obj.type.name != "GameObject":
                continue
            try:
                name = str(obj.read().m_Name)
            except Exception:
                continue
            if re.fullmatch(r"e_[A-Za-z0-9_]+", name):
                enemy_objects.setdefault(name, obj)
        extracted = {}
        for enemy_id in sorted(enemy_objects):
            try:
                definition = self._extract_enemy(enemy_id, enemy_objects[enemy_id])
            except (Exception, SystemExit) as exc:
                print(f"SKIP ENEMY {enemy_id}: {exc}")
                continue
            self.enemy_definitions.append(definition)
            extracted[enemy_id] = definition
        pool = ["e_miner01", "e_miner02", "e_miner03", "e_yeti01"]
        fixture_ids = [eid for eid in pool if eid in extracted]
        if not fixture_ids:
            fixture_ids = sorted(extracted)[:2]
        self.enemy_fixtures = [
            {"definition": fixture_ids[0], "position": [14.0, 15.5]},
            {"definition": fixture_ids[1] if len(fixture_ids) > 1 else fixture_ids[0],
             "position": [26.0, 15.5]},
        ]

        # Recovered pet prefabs. The base ResourcesUtil path suffix is not
        # serialized, so only prefabs that actually exist in the bundles are
        # emitted; no pet skin ids are fabricated.
        pet_objects = {}
        pet_candidates = {}
        pet_names = {
            "pet", "wolf1", "wolf2", "pet_datura", "pet_eator", "battery",
            "npc_snowman", "easter_rabbit", "wild_pet_battle",
        }
        for obj in self.env.objects:
            if obj.type.name != "GameObject":
                continue
            try:
                name = str(obj.read().m_Name)
            except Exception:
                continue
            if name in pet_names:
                pet_candidates.setdefault(name, []).append(obj)
        for name, candidates in pet_candidates.items():
            best = None
            best_mono = -1
            for candidate in candidates:
                try:
                    components = [c.component.type.name
                                  for c in candidate.read().m_Component]
                except Exception:
                    continue
                mono_count = components.count("MonoBehaviour")
                if "Animator" in components and mono_count > best_mono:
                    best = candidate
                    best_mono = mono_count
            if best is not None:
                pet_objects[name] = best
        pet_labels = {
            "pet": "Cat", "wolf1": "Wolf", "wolf2": "Snow Wolf",
            "pet_datura": "Datura", "pet_eator": "Eater", "battery": "Battery",
            "npc_snowman": "Snowman", "easter_rabbit": "Easter Rabbit",
            "wild_pet_battle": "Wild Pet",
        }
        self.pet_definitions = []
        for pet_id in sorted(pet_objects):
            try:
                definition = self._extract_pet(pet_id, pet_objects[pet_id])
            except (Exception, SystemExit) as exc:
                print(f"SKIP PET {pet_id}: {exc}")
                continue
            definition["display_name"] = pet_labels.get(pet_id, pet_id)
            self.pet_definitions.append(definition)

    def extract_player_animations(self, player_root, character_id="c00") -> None:
        animator_component = None
        for component in player_root.read().m_Component:
            if component.component.type.name == "Animator":
                animator_component = component.component
                break
        if animator_component is None:
            raise SystemExit(f"{character_id} has no Animator component")

        animator = animator_component.read()
        controller_ptr = animator.m_Controller
        if controller_ptr is None or controller_ptr.path_id == 0:
            raise SystemExit(f"{character_id} Animator has no controller")
        controller = controller_ptr.read()
        state_names = {2215514825: "ide", 1349952704: "run",
                       3978643748: "dead", 4116247876: "hit"}
        selected_states = {
            "ide": f"{character_id}_idle",
            "run": f"{character_id}_run",
            "dead": f"{character_id}_dead",
            "hit": f"{character_id}_hit",
        }

        states_by_name: dict[str, tuple[object, int, str]] = {}
        for sm_index, sm_offset in enumerate(
                controller.m_Controller.m_StateMachineArray):
            state_machine = sm_offset.data
            layer_name = ("Base Layer" if sm_index == 0 else
                          "HitLayer" if sm_index == 1 else f"Layer {sm_index}")
            for state_offset in state_machine.m_StateConstantArray:
                state = state_offset.data
                state_name = state_names.get(int(state.m_NameID))
                if state_name in selected_states and state_name not in states_by_name:
                    states_by_name[state_name] = (state, sm_index, layer_name)

        for state_name, native_id in selected_states.items():
            if state_name not in states_by_name:
                if state_name == "hit":
                    continue
                raise SystemExit(
                    f"{character_id} controller has no selected {state_name} state")
            state, state_machine_index, layer_name = states_by_name[state_name]
            clip_index = int(
                state.m_BlendTreeConstantArray[0].data.m_NodeArray[0].data.m_ClipID)
            clip_ptr = controller.m_AnimationClips[clip_index]
            clip = clip_ptr.read()

            loop = bool(state.m_Loop) and bool(clip.m_MuscleClip.m_LoopTime)
            duration = float(
                clip.m_MuscleClip.m_StopTime - clip.m_MuscleClip.m_StartTime)
            sample_rate = float(clip.m_SampleRate)

            bindings = list(clip.m_ClipBindingConstant.genericBindings)
            target_hashes = [
                zlib.crc32(PLAYER_BODY_PATH.encode("utf-8")) & 0xFFFFFFFF,
                zlib.crc32(b"img/body") & 0xFFFFFFFF,
                zlib.crc32(b"c00/img") & 0xFFFFFFFF,
            ]
            target_hash = target_hashes[0]
            target_index = None
            for index, binding in enumerate(bindings):
                if (int(binding.path) in target_hashes and
                        int(binding.customType) == 23 and
                        int(binding.isPPtrCurve) == 1 and
                        int(getattr(binding, "typeID", 0) or 0) == 212):
                    target_hash = int(binding.path)
                    target_index = index
                    break
            if target_index is None:
                if state_name in ("dead", "hit"):
                    continue
                raise SystemExit(
                    f"{character_id}: no SpriteRenderer.m_Sprite binding for "
                    f"{PLAYER_BODY_PATH} ({state_name})")
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
                if getattr(mapping_ptr, "path_id", 0) == 0:
                    mapping_sprite_ids.append("")
                    continue
                sprite_id, _ = self.ensure_sprite_asset(mapping_ptr)
                mapping_sprite_ids.append(sprite_id)

            source_keys: list[dict] = []
            for time, value in curve_keys:
                rounded = int(round(value))
                if abs(value - rounded) > 1e-4 or not (0 <= rounded < len(mapping)):
                    raise SystemExit(
                        f"pptr curve value {value} does not index pptrCurveMapping")
                if not mapping_sprite_ids[rounded]:
                    continue
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
                    "character_id": character_id,
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
                    "state_machine_index": str(state_machine_index),
                    "state_machine_layer": layer_name,
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
    def _rect_from_collider_xy(position_x, position_y, collider):
        center_x = float(position_x) + float(collider.m_Offset.x)
        center_y = float(position_y) + float(collider.m_Offset.y)
        half_x = float(collider.m_Size.x) * 0.5
        half_y = float(collider.m_Size.y) * 0.5
        return [center_x - half_x, center_y - half_y,
                center_x + half_x, center_y + half_y]

    @staticmethod
    def _rect_from_collider(position, collider):
        return SceneBuilder._rect_from_collider_xy(
            float(position.x), float(position.y), collider)


    def _extract_level_themes(self):
        """Recover MapManagerLevel floor/wall surface themes.

        RGRoomX.CreateFloor/CreateWall read these arrays from the singleton
        level manager, not from the PatternRoom prefab. The theme catalogue is
        therefore shared by every room and only the selected campaign level
        changes the surface sprites.
        """
        def manager_level(name):
            if name == "map_G1":
                return "3-6", "A"
            match = re.fullmatch(r"map_([ABC])(\d+)", name)
            if match is None:
                return None, None
            branch = match.group(1)
            index = int(match.group(2))
            if branch == "A":
                if 1 <= index <= 5:
                    return f"1-{index}", branch
                if 6 <= index <= 10:
                    return f"2-{index - 5}", branch
                if 11 <= index <= 15:
                    return f"3-{index - 10}", branch
            elif branch == "B":
                if 1 <= index <= 5:
                    return f"1-{index}", branch
                if 6 <= index <= 10:
                    return f"2-{index - 5}", branch
                if 11 <= index <= 15:
                    return f"3-{index - 10}", branch
            elif branch == "C":
                if 1 <= index <= 5:
                    return f"1-{index}", branch
                if 6 <= index <= 10:
                    return f"2-{index - 5}", branch
            return None, None

        managers = {}
        for obj in self.env.objects:
            if obj.type.name != "GameObject":
                continue
            try:
                game_object = obj.read()
            except Exception:
                continue
            name = str(getattr(game_object, "m_Name", ""))
            if not re.fullmatch(r"map_(?:[ABC]\d+|G1)", name):
                continue
            for component in game_object.m_Component:
                if component.component.type.name != "MonoBehaviour":
                    continue
                try:
                    candidate = component.component.read()
                except Exception:
                    continue
                if hasattr(candidate, "floor_list") and hasattr(candidate, "wall_list"):
                    managers[name] = candidate
                    break

        def prefab_parts(pointer):
            go = pointer.read()
            transform = next(
                (c.component.read() for c in go.m_Component
                 if c.component.type.name == "Transform"), None)
            if transform is None:
                return []
            records = []

            def collect(tf, is_root=False):
                child_go = tf.m_GameObject.read()
                offset = [0.0, 0.0] if is_root else [
                    float(tf.m_LocalPosition.x), float(tf.m_LocalPosition.y)]
                for component in child_go.m_Component:
                    if component.component.type.name != "SpriteRenderer":
                        continue
                    sr = component.component.read()
                    sprite_id, _ = self.ensure_sprite_asset(sr.m_Sprite)
                    records.append({
                        "sprite": sprite_id,
                        "offset": offset,
                        "rotation": 0.0,
                        "scale": [float(tf.m_LocalScale.x),
                                  float(tf.m_LocalScale.y)],
                        "color": color_bytes(sr.m_Color),
                        "sorting_layer": int(sr.m_SortingLayer),
                        "sorting_order": int(sr.m_SortingOrder),
                        "flip_x": bool(sr.m_FlipX),
                        "flip_y": bool(sr.m_FlipY),
                    })
                for child in getattr(tf, "m_Children", []) or []:
                    collect(child.read())

            collect(transform, True)
            return records

        themes = []
        for name in sorted(managers):
            level, branch = manager_level(name)
            if level is None:
                continue
            manager = managers[name]
            floors = []
            for pointer in list(getattr(manager, "floor_list", []) or []):
                try:
                    parts = prefab_parts(pointer)
                except Exception:
                    continue
                if parts:
                    floors.append({"sprite": parts[0]})
            walls = []
            for pointer in list(getattr(manager, "wall_list", []) or []):
                try:
                    parts = prefab_parts(pointer)
                except Exception:
                    continue
                if parts:
                    walls.append({"sprites": parts})
            obstacles = []
            for pointer in list(getattr(manager, "obstacle_list", []) or []):
                try:
                    parts = prefab_parts(pointer)
                except Exception:
                    continue
                if parts:
                    obstacles.append({"sprites": parts})
            if not floors or not walls:
                continue
            themes.append({
                "level": level,
                "branch": branch,
                "manager": name,
                "use_new_floor_pattern": bool(getattr(
                    manager, "use_new_floor_pattern", False)),
                "floors": floors,
                "walls": walls,
                "obstacles": obstacles,
            })
        return themes

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

        def collect(transform, path, is_root=False):
            go = transform.m_GameObject.read()
            # CreateWall instantiates the prefab and overwrites the root local
            # position with the wall-cell coordinate. The serialized prefab asset
            # retains an editor position (e.g. 20,20); it is NOT a sprite offset.
            offset_x = 0.0 if is_root else float(transform.m_LocalPosition.x)
            offset_y = 0.0 if is_root else float(transform.m_LocalPosition.y)
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
                    "local_x": offset_x,
                    "local_y": offset_y,
                })
            for child in transform.m_Children:
                collect(child.read(), f"{path}/{go.m_Name}")

        collect(wall_tf, wall_go.m_Name, is_root=True)
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

    def _obstacle_prefabs(self):
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

        def collect(transform, parent_pos, parent_angle, parent_scale, path,
                    is_root=False):
            go = transform.m_GameObject.read()
            local = transform.m_LocalPosition
            local_angle = qangle(transform.m_LocalRotation)
            local_scale = transform.m_LocalScale
            offset_x = 0.0 if is_root else float(local.x)
            offset_y = 0.0 if is_root else float(local.y)
            cos_a = math.cos(math.radians(parent_angle))
            sin_a = math.sin(math.radians(parent_angle))
            world_x = parent_pos[0] + (
                cos_a * offset_x * parent_scale[0] -
                sin_a * offset_y * parent_scale[1])
            world_y = parent_pos[1] + (
                sin_a * offset_x * parent_scale[0] +
                cos_a * offset_y * parent_scale[1])
            world_angle = parent_angle + local_angle
            world_scale = (
                parent_scale[0] * float(local_scale.x),
                parent_scale[1] * float(local_scale.y),
            )
            current_path = f"{path}/{go.m_Name}"
            records = []
            for component in go.m_Component:
                if component.component.type.name != "SpriteRenderer":
                    continue
                renderer = component.component.read()
                if not getattr(renderer, "m_Enabled", True):
                    continue
                records.append({
                    "path": current_path,
                    "sprite_ptr": renderer.m_Sprite,
                    "sorting_layer": int(renderer.m_SortingLayer),
                    "sorting_order": int(renderer.m_SortingOrder),
                    "color": color_bytes(renderer.m_Color),
                    "flip_x": bool(renderer.m_FlipX),
                    "flip_y": bool(renderer.m_FlipY),
                    "local_x": world_x,
                    "local_y": world_y,
                    "angle": world_angle,
                    "scale": [world_scale[0], world_scale[1]],
                })
            for child in transform.m_Children:
                records.extend(collect(child.read(), (world_x, world_y),
                                       world_angle, world_scale, current_path))
            return records

        prefabs = []
        for index, prefab_ptr in enumerate(manager.obstacle_list):
            game_object = prefab_ptr.read()
            transform = next(
                component.component.read() for component in game_object.m_Component
                if component.component.type.name == "Transform")
            collider, _ = self._component(game_object, "BoxCollider2D")
            breakable = False
            box_hp = 0
            for component in game_object.m_Component:
                if component.component.type.name != "MonoBehaviour":
                    continue
                try:
                    candidate = component.component.read()
                    script = (candidate.m_Script.read()
                              if getattr(candidate, "m_Script", None) and
                              candidate.m_Script.path_id else None)
                    class_name = getattr(script, "m_ClassName", "")
                except Exception:
                    continue
                if class_name == "RGBox":
                    breakable = True
                    box_hp = int(getattr(candidate, "hp", 0) or 0)
                    break
            prefabs.append({
                "index": index,
                "name": getattr(game_object, "m_Name", ""),
                "path_id": str(prefab_ptr.path_id),
                "serialized_layer": int(game_object.m_Layer),
                "collider_size": (
                    [float(collider.m_Size.x), float(collider.m_Size.y)]
                    if collider is not None else None),
                "collider_offset": (
                    [float(collider.m_Offset.x), float(collider.m_Offset.y)]
                    if collider is not None else None),
                "collider_trigger": (
                    bool(collider.m_IsTrigger) if collider is not None else None),
                "breakable": breakable,
                "box_hp": box_hp,
                "visuals": collect(transform, (0.0, 0.0), 0.0, (1.0, 1.0),
                                   getattr(game_object, "m_Name", ""),
                                   is_root=True),
            })
        return prefabs

    def _obstacle_metadata(self):
        prefabs = self._obstacle_prefabs()
        variants = []
        for prefab in prefabs:
            visuals = []
            for visual in prefab["visuals"]:
                sprite_id, _ = self.ensure_sprite_asset(visual["sprite_ptr"])
                visuals.append({
                    "sprite": sprite_id,
                    "offset": [float(visual["local_x"]),
                               float(visual["local_y"])],
                    "rotation": math.radians(float(visual["angle"])),
                    "scale": [float(visual["scale"][0]),
                              float(visual["scale"][1])],
                    "color": visual["color"],
                    "sorting_layer": int(visual["sorting_layer"]),
                    "sorting_order": int(visual["sorting_order"]),
                    "flip_x": bool(visual["flip_x"]),
                    "flip_y": bool(visual["flip_y"]),
                })
            collider = None
            if (prefab["collider_size"] is not None and
                    prefab["collider_offset"] is not None):
                collider = {
                    "size": [float(prefab["collider_size"][0]),
                             float(prefab["collider_size"][1])],
                    "offset": [float(prefab["collider_offset"][0]),
                               float(prefab["collider_offset"][1])],
                    "trigger": bool(prefab["collider_trigger"]),
                }
            variants.append({
                "index": int(prefab["index"]),
                "name": prefab["name"],
                "path_id": prefab["path_id"],
                "collider": collider,
                "breakable": bool(prefab["breakable"]),
                "hp": int(prefab["box_hp"]),
                "visuals": visuals,
            })
        markers = []
        for marker in self.object_markers:
            index = int(marker["obj_index"])
            if not (0 <= index < len(variants)):
                raise SystemExit(
                    f"RGObjectSkin obj_index {index} is outside "
                    f"MapManagerLevel.obstacle_list")
            markers.append({
                "position": [float(marker["position"][0]),
                             float(marker["position"][1])],
                "obj_index": index,
                "name": marker["name"],
                "path": marker["path"],
            })
        return variants, markers

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
            raise SystemExit(f"{self.room_id} has no RGRoomX MonoBehaviour")

        width = int(room_mb.room_width)
        height = int(room_mb.room_height)
        offset_x = (ROOM_MAP_CELL_SIZE - width) // 2
        offset_y = (ROOM_MAP_CELL_SIZE - height) // 2
        if width not in (11, 13, 15, 19, 21) or height not in (11, 13, 15, 19, 21):
            raise SystemExit(
                f"unexpected {self.room_id} room size {width}x{height}; "
                "recovered sizes are 11/15/21")

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
                raise SystemExit(f"{self.room_id} missing {aisle_name}")
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
            door_animation = None
            if animator is not None and animator.m_Controller:
                controller = animator.m_Controller.read()
                controller_name = getattr(controller, "m_Name", None)
                for clip_ptr in controller.m_AnimationClips:
                    clip_names.append(getattr(clip_ptr.read(), "m_Name", ""))
                door_animation = self._extract_door_animation(
                    controller, animator.m_Controller)
                door_animation["open"]["source"]["clip_path_id"] = str(
                    next(p.path_id for p in controller.m_AnimationClips
                         if getattr(p.read(), "m_Name", "") == "door_open"))
                close_name = next(
                    name for name in ("door_close", "door_close2")
                    if name in clip_names)
                door_animation["close"]["source"]["clip_path_id"] = str(
                    next(p.path_id for p in controller.m_AnimationClips
                         if getattr(p.read(), "m_Name", "") == close_name))

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
                **({"open_animation": door_animation["open"],
                    "close_animation": door_animation["close"]}
                   if door_animation else {}),
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
        obstacle_variants, obstacle_markers = self._obstacle_metadata()
        for instance in self.instances:
            path = instance.get("path", "")
            for aisle_name, (side, _, _) in AISLE_DIRECTIONS.items():
                if f"/{aisle_name}/door/" in path:
                    instance["door"] = side
                    break
            if not instance.get("door"):
                continue
            parts = path.split("/")
            if "door" not in parts:
                raise SystemExit(f"door instance path is malformed: {path}")
            remainder = parts[parts.index("door") + 1:]
            if not remainder:
                raise SystemExit(f"door instance path has no leaf: {path}")
            leaf = remainder[0]
            instance["door_leaf"] = leaf
            if len(remainder) == 1:
                instance["door_role"] = "leaf"
            elif remainder[1] == "shadow":
                instance["door_role"] = "shadow"
            elif remainder[1] == "d":
                instance["door_role"] = "ornament"
            else:
                raise SystemExit(
                    f"unexpected door instance hierarchy: {path}")

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
                    **({"open_animation": opening["open_animation"],
                        "close_animation": opening["close_animation"]}
                       if "open_animation" in opening else {}),
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
                "colliders, RGObjectSkin -> obstacle_list replacement and "
                "obstacle prefab colliders; NATIVE TEST entrance/door state "
                "and map-cell boundary"),
        }

        room_parts = self.room_id.split("_")
        stage = int(room_parts[0][1:]) if room_parts and room_parts[0].startswith("r") and room_parts[0][1:].isdigit() else 0
        number = int(room_parts[1]) if len(room_parts) > 1 and room_parts[1].isdigit() else 0
        room_type = int(getattr(room_mb, "room_type", 0))
        room_type_name = {0: "start", 1: "normal", 2: "end",
                          3: "chest", 4: "special"}.get(room_type, "unknown")
        topology = {
            "source_room": self.bundle_name,
            "room": {
                "type": room_type,
                "type_name": room_type_name,
                "stage": stage,
                "number": number,
                "label": f"{stage}-{number}" if stage > 0 and number > 0 else self.room_id,
                "clear_label": "clear",
                "classification": "CONFIRMED RGRoomX.room_type; stage/number derived from recovered PatternRoom id",
            },
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
            "obstacle_prefabs": [
                {
                    "index": variant["index"],
                    "name": variant["name"],
                    "path_id": variant["path_id"],
                    "collider_size": (
                        variant["collider"]["size"]
                        if variant["collider"] else None),
                    "collider_offset": (
                        variant["collider"]["offset"]
                        if variant["collider"] else None),
                    "collider_trigger": (
                        variant["collider"]["trigger"]
                        if variant["collider"] else None),
                }
                for variant in obstacle_variants
            ],
            "obstacle_markers": obstacle_markers,
            "obstacle_solid_count": sum(
                1 for marker in obstacle_markers
                if obstacle_variants[marker["obj_index"]]["collider"] is not None
                and not obstacle_variants[marker["obj_index"]]["collider"]["trigger"]),
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

        self.obstacles = {
            "markers": obstacle_markers,
            "variants": obstacle_variants,
            "default_variant": 0,
            "classification": (
                "CONFIRMED MapManagerLevel.obstacle_list prefabs and "
                "RGObjectSkin marker positions; runtime variant switching is "
                "a NATIVE TEST FEATURE"),
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
            "obstacle_cells": [
                [int(round(marker["position"][0])),
                 int(round(marker["position"][1]))]
                for marker in obstacle_markers
            ],
            "obstacle_prefabs": [
                {
                    "index": variant["index"],
                    "name": variant["name"],
                    "path_id": variant["path_id"],
                    "collider_size": (
                        variant["collider"]["size"]
                        if variant["collider"] else None),
                    "collider_offset": (
                        variant["collider"]["offset"]
                        if variant["collider"] else None),
                    "collider_trigger": (
                        variant["collider"]["trigger"]
                        if variant["collider"] else None),
                }
                for variant in obstacle_variants
            ],
            "spawn": [float(self.args.spawn[0]), float(self.args.spawn[1])],
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
            room_walls, corridor_walls,
            [(int(round(marker["position"][0])),
              int(round(marker["position"][1])))
             for marker in obstacle_markers])
        self.topology = topology
        self.wall_map = wall_map
        return collision

    @staticmethod
    def _wall_map_ascii(width, height, offset_x, offset_y,
                        room_walls, corridor_walls, obstacle_cells=()):
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
        for (x, y) in obstacle_cells:
            if 0 <= x < ROOM_MAP_CELL_SIZE and 0 <= y < ROOM_MAP_CELL_SIZE:
                grid[y][x] = "O"
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
        lines = ["".join(row).rstrip() for row in reversed(grid)]
        return "\n".join(lines) + "\n"

    def walk_room(self, tf_ptr, parent_pos, parent_angle, parent_scale, path):
        transform = tf_ptr.read()
        game_object = transform.m_GameObject.read()
        name = getattr(game_object, "m_Name", "")
        marker_index = self._object_skin_index(game_object)
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
        if marker_index is not None:
            # RGRoomX.CreateRoomObstacle destroys these RGObjectSkin markers
            # after instantiating obstacle_list[obj_index]. Do not render the
            # placeholder skin; the runtime replacement is added below.
            self.object_markers.append({
                "name": name,
                "path": current_path,
                "obj_index": marker_index,
                "position": [world_x, world_y],
                "rotation": world_angle,
            })
            return
        for component in game_object.m_Component:
            if component.component.type.name == "SpriteRenderer":
                sr = component.component.read()
                if getattr(sr, "m_Enabled", True):
                    self.add_sprite_renderer(
                        sr, (world_x, world_y), world_angle, world_scale, current_path)
        for child in transform.m_Children:
            self.walk_room(child, (world_x, world_y), world_angle, world_scale, current_path)

    @staticmethod
    def _object_skin_index(game_object):
        for component in game_object.m_Component:
            if component.component.type.name != "MonoBehaviour":
                continue
            try:
                data = component.component.read()
            except Exception:
                continue
            if hasattr(data, "obj_index"):
                return int(data.obj_index)
        return None

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
        character_roots: dict[str, object] = {}
        character_candidates: dict[str, list] = {}

        def has_body_binding(root) -> bool:
            try:
                animator = next(
                    c.component.read() for c in root.read().m_Component
                    if c.component.type.name == "Animator")
                controller = animator.m_Controller.read()
                for state_offset in (
                        controller.m_Controller.m_StateMachineArray[0].data
                        .m_StateConstantArray):
                    state = state_offset.data
                    if int(state.m_NameID) != 2215514825:
                        continue
                    clip_index = int(
                        state.m_BlendTreeConstantArray[0].data
                        .m_NodeArray[0].data.m_ClipID)
                    clip = controller.m_AnimationClips[clip_index].read()
                    target_hashes = {
                        zlib.crc32(PLAYER_BODY_PATH.encode("utf-8")) & 0xFFFFFFFF,
                        zlib.crc32(b"img/body") & 0xFFFFFFFF,
                        zlib.crc32(b"c00/img") & 0xFFFFFFFF,
                    }
                    for binding in clip.m_ClipBindingConstant.genericBindings:
                        if (int(binding.path) in target_hashes and
                                int(binding.customType) == 23 and
                                int(binding.isPPtrCurve) == 1 and
                                int(getattr(binding, "typeID", 0) or 0) == 212):
                            return True
            except Exception:
                return False
            return False
        for obj in self.env.objects:
            if obj.type.name != "GameObject":
                continue
            data = obj.read()
            name = getattr(data, "m_Name", "")
            if name == self.root_name:
                components = [c.component.type.name for c in data.m_Component]
                if "MonoBehaviour" not in components:
                    continue
                is_room = False
                for component in data.m_Component:
                    if component.component.type.name != "MonoBehaviour":
                        continue
                    try:
                        candidate_data = component.component.read()
                    except Exception:
                        continue
                    if hasattr(candidate_data, "room_width"):
                        is_room = True
                        break
                if is_room:
                    room_root = obj
            elif re.fullmatch(r"c\d{2}", name):
                # Character prefabs live in the common bundle. Room bundles can
                # contain same-named skin variants whose body binding differs.
                if self.cab_to_bundle.get(
                        getattr(obj.assets_file, "name", "")) != "common":
                    continue
                components = [c.component.type.name for c in data.m_Component]
                if "Animator" not in components:
                    continue
                candidates = character_candidates.setdefault(name, [])
                if all(candidate.path_id != obj.path_id for candidate in candidates):
                    candidates.append(obj)
                good = [candidate for candidate in candidates
                        if has_body_binding(candidate)]
                if good:
                    character_roots[name] = good[0]
                else:
                    character_roots[name] = candidates[0]
        if room_root is None:
            raise SystemExit(f"{self.room_id} root GameObject not found")
        if "c00" not in character_roots:
            raise SystemExit("c00 character root GameObject not found")
        player_root = character_roots["c00"]

        room_transform = next(
            c.component for c in room_root.read().m_Component
            if c.component.type.name == "Transform")
        self.walk_room(room_transform, (0.0, 0.0), 0.0, (1.0, 1.0), self.room_id)
        collision = self._extract_topology(room_root, room_transform)

        self._combat_metadata()

        # Boss rooms use BossCreator.boss rather than EnemyMaker. Extract the
        # actual boss prefab as the room's enemy definition/fixture.
        def find_boss_creator(tf, depth=0):
            if depth > 4:
                return None
            if hasattr(tf, "read"):
                tf = tf.read()
            go = tf.m_GameObject.read()
            for component in go.m_Component:
                if component.component.type.name != "MonoBehaviour":
                    continue
                try:
                    candidate = component.component.read()
                    script = (candidate.m_Script.read()
                              if getattr(candidate, "m_Script", None) and
                              candidate.m_Script.path_id else None)
                    if getattr(script, "m_ClassName", "") == "BossCreator":
                        return candidate
                except Exception:
                    continue
            for child in getattr(tf, "m_Children", []) or []:
                found = find_boss_creator(child.read(), depth + 1)
                if found is not None:
                    return found
            return None

        boss_creator = find_boss_creator(room_transform)
        if boss_creator is not None:
            boss_ptr = getattr(boss_creator, "boss", None)
            if boss_ptr is not None and getattr(boss_ptr, "path_id", 0) != 0:
                try:
                    boss_go = boss_ptr.read()
                    boss_definition = self._extract_enemy(
                        str(boss_go.m_Name), boss_ptr)
                    self.enemy_definitions.append(boss_definition)
                    self.enemy_fixtures = [{
                        "definition": boss_definition["id"],
                        "position": [20.0, 15.0],
                    }]
                    print(f"BOSS {boss_definition['id']}: extracted")
                except Exception as exc:
                    print(f"SKIP BOSS: {exc}")

        player_transform = next(
            c.component for c in player_root.read().m_Component
            if c.component.type.name == "Transform").read()
        spawn = self.args.spawn
        for child in player_transform.m_Children:
            self.walk_player(child, spawn, 0.0, (1.0, 1.0), "c00")

        for character_id, root in sorted(character_roots.items()):
            body_sprite_ptr = self._player_body_sprite(root)
            if body_sprite_ptr is None:
                raise SystemExit(f"{character_id} has no body SpriteRenderer")
            body_sprite_id, _ = self.ensure_sprite_asset(body_sprite_ptr)
            animator = next(
                c.component.read() for c in root.read().m_Component
                if c.component.type.name == "Animator")
            controller = animator.m_Controller.read()
            self.extract_player_animations(root, character_id)
            state_machine = controller.m_Controller.m_StateMachineArray[0].data
            dead_sprite = self._character_dead_sprite(controller, state_machine)
            role = None
            for component in root.read().m_Component:
                if component.component.type.name != "MonoBehaviour":
                    continue
                try:
                    candidate = component.component.read()
                except Exception:
                    continue
                if hasattr(candidate, "max_hp") and hasattr(candidate, "max_armor"):
                    role = candidate
                    break
            self.characters.append({
                "id": character_id,
                "max_hp": int(getattr(role, "max_hp", 0)) if role else 0,
                "max_armor": int(getattr(role, "max_armor", 0)) if role else 0,
                "max_energy": int(getattr(role, "max_energy", 0)) if role else 0,
                "speed": float(getattr(role, "speed", 0.0)) if role else 0.0,
                "speed_rate": float(getattr(role, "speed_rate", 0.0)) if role else 0.0,
                "controller_name": getattr(controller, "m_Name", ""),
                "controller_path_id": str(animator.m_Controller.path_id),
                "source_path_id": str(root.path_id),
                "body_sprite": body_sprite_id,
                "dead_sprite": dead_sprite,
                "idle_animation": f"{character_id}_idle",
                "run_animation": f"{character_id}_run",
                "death_animation": f"{character_id}_dead",
                "hit_animation": f"{character_id}_hit",
                "starting_weapon": str(
                    CHARACTER_LOADOUTS.get(character_id, {}).get(
                        "starting_weapon", "")),
                "classification": (
                    "CONFIRMED serialized c00..c14 character prefabs from the "
                    "common bundle; display names are not proven"),
            })

        return {
            "id": f"{self.room_id}_visual",
            "room": self.topology.get("room", {}),
            "source_room": self.bundle_name,
            "source_player": "common/c00",
            "spawn": [spawn[0], spawn[1]],
            "characters": self.characters,
            "player_stats": self.player_stats,
            "weapons": self.weapons,
            "level_themes": self._extract_level_themes(),
            "enemy_definitions": self.enemy_definitions,
            "enemy_fixtures": self.enemy_fixtures,
            "pet_definitions": self.pet_definitions,
            "spawn_classification": (
                "NATIVE TEST CONFIGURATION: player starts just inside the "
                "south door on free floor; the recovered c00 prefab transform "
                "(20,20) overlaps the central RGObjectSkin -> wall603 obstacle "
                "block in r1_1"),
            "camera": {
                "position": [spawn[0], spawn[1]],
                "orthographic_size": 10.5,
                "viewport": [1920.0, 1080.0],
            },
            "textures": list(self.textures.values()),
            "sprites": [self.sprites[key] for key in self.sprites],
            "instances": self.instances,
            "obstacles": self.obstacles,
            "topology": self.topology,
            "collision": collision,
            "animations": [
                f"animations/{native_id}.json"
                for native_id in sorted(self.animations)
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
    parser.add_argument("--room", default="r1_1")
    parser.add_argument("--bundle", default=None,
                        help="AssetBundle name; defaults to room/<room>")
    parser.add_argument("--root-name", default=None,
                        help="root GameObject name; defaults to --room")
    parser.add_argument("--spawn", type=float, nargs=2, default=(20.0, 15.0))
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
        f"{args.room} recovered room topology (ASCII verification map)\n"
        "Legend: # room wall  + corridor wall  D five-cell door opening  "
        "O recovered wall603 obstacle  ':' corridor floor  '.' room floor\n"
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
    source_bundle_name = args.bundle or f"room/{args.room}"
    bundles = {
        source_bundle_name: source_sha256(
            bundle_path(source_bundle_name, args.bundle_root)),
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
        "character_count": len(scene["characters"]),
        "player_stats": scene["player_stats"],
        "weapon_count": len(scene["weapons"]),
        "enemy_definition_count": len(scene["enemy_definitions"]),
        "enemy_fixture_count": len(scene["enemy_fixtures"]),
        "obstacle_variant_count": len(scene["obstacles"]["variants"]),
        "obstacle_marker_count": len(scene["obstacles"]["markers"]),
        "animation_keyframe_count": sum(
            len(track["keys"])
            for animation in builder.animations.values()
            for track in animation.get("tracks", [])),
        "door_animation_keyframe_count": sum(
            sum(len(clip[name]) for name in (
                "leaf_extent", "shadow_visible", "ornament_visible",
                "collider_trigger", "collider_size_x", "collider_size_y",
                "leaf_sprites"))
            for door in scene["collision"]["doors"]
            for clip in (door["open_animation"], door["close_animation"])),
        "animations": animation_records,
        "topology": {
            "wall_cell_count": scene["topology"]["wall_cell_count"],
            "corridor_wall_cell_count": scene["topology"]["corridor_wall_cell_count"],
            "obstacle_solid_count": scene["topology"]["obstacle_solid_count"],
            "floor_cell_count": scene["topology"]["floor_cell_count"],
            "aisle_cell_count": scene["topology"]["aisle_cell_count"],
            "opening_count": len(scene["topology"]["openings"]),
            "wall_map_json": str(args.wall_map.relative_to(ROOT)),
            "wall_map_txt": str(wall_map_txt.relative_to(ROOT)),
            "wall_map_sha256": wall_map_sha,
        },
        "collision": scene["collision"],
        "notes": f"{args.room} native test reconstruction with recovered CreateWall/door geometry, door_open/door_close Animator keyframes, 15 recovered character prefabs with ide/run clips, 12 obstacle_list variants, weapon_000/bullet_0 combat metadata, two e_slime01 test fixtures, and no proprietary pixels committed.",
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
