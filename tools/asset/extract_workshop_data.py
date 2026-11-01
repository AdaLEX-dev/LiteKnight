#!/usr/bin/env python3
"""Extract Soul Knight 2.6.8 workshop materials and forge recipes.

Outputs (only these files):
    analysis/assets/workshop/materials.json
    analysis/assets/workshop/materials.md
    analysis/assets/workshop/weapon_materials.json
    analysis/assets/workshop/weapon_materials.md

The extractor reads the local reference tree through UnityPy. It parses the
serialized WeaponMaterial / CustomWeaponMaterial components, the Odin
SerializedScriptableObject ``material_config`` (DefaultWeaponMaterial), and the
ItemInfo TextAsset. Nothing is inferred from display strings.

Run with the repository asset venv:
    tools/asset/.venv/bin/python tools/asset/extract_workshop_data.py
"""

from __future__ import annotations

import hashlib
import json
import math
import re
import struct
import sys
from pathlib import Path
from typing import Any, Iterable

try:
    import UnityPy
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        "UnityPy is required; run with tools/asset/.venv/bin/python"
    ) from exc

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "analysis/apktool_out/assets/bin/Data"
COMMON = ROOT / "analysis/apktool_out/assets/AssetBundles/common"
ITEM_INFO_FILE = DATA / "059758e6342e4a446b033a7ff6f6b762"
MATERIAL_CONFIG_FILE = DATA / "ab5420ac0fce9e34f99d079b6d8501b1"
LOCALIZATION_FILE = DATA / "65e9ec86e4ca50c478ad2b19c9878f51"
LEVEL2 = DATA / "level2"
GLOBAL_ASSETS = DATA / "globalgamemanagers.assets"
WEAPON_CATALOGUE = ROOT / "analysis/assets/weapon_catalogue.json"
OUT_DIR = ROOT / "analysis/assets/workshop"

MATERIAL_PREFIX = "material_"
UI_TEMPLATE_NAMES = {"material_panel", "material_proto"}


# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def align4(value: int) -> int:
    return (value + 3) & ~3


def as_pptr_list(pptrs: Iterable[Any]) -> list[tuple[int, int]]:
    return [(int(p.file_id), int(p.path_id)) for p in pptr_list(pptrs)]


def pptr_list(pptrs: Iterable[Any]) -> list[Any]:
    return list(pptrs or [])


def ordered_unique(values: Iterable[str]) -> list[str]:
    seen: set[str] = set()
    result: list[str] = []
    for value in values:
        if value not in seen:
            seen.add(value)
            result.append(value)
    return result


# ---------------------------------------------------------------------------
# Unity object graph view
# ---------------------------------------------------------------------------


class AssetView:
    """A deterministic lookup view over one UnityPy asset file."""

    def __init__(self, path: Path):
        self.path = path
        self.env = UnityPy.load(str(path))
        self.objects = list(self.env.objects)
        self.object_by_pid: dict[int, Any] = {o.path_id: o for o in self.objects}
        self.go_by_pid: dict[int, Any] = {}
        self.go_name_by_pid: dict[int, str] = {}
        self.tr_by_pid: dict[int, Any] = {}
        self.go_by_tr: dict[int, int] = {}
        self.go_tr: dict[int, int] = {}
        self.go_parent_tr: dict[int, int] = {}

        for obj in self.objects:
            if obj.type.name != "GameObject":
                continue
            try:
                go = obj.read()
            except Exception:
                continue
            self.go_by_pid[obj.path_id] = go
            self.go_name_by_pid[obj.path_id] = go.m_Name
            for comp in go.m_Component:
                if comp.component.type.name not in ("Transform", "RectTransform"):
                    continue
                tr_pid = comp.component.path_id
                self.go_tr[obj.path_id] = tr_pid
                self.go_by_tr[tr_pid] = obj.path_id
                try:
                    tr = comp.component.read()
                    father = getattr(getattr(tr, "m_Father", None), "m_PathID", 0) or 0
                    self.go_parent_tr[obj.path_id] = father
                    self.tr_by_pid[tr_pid] = tr
                except Exception:
                    self.go_parent_tr[obj.path_id] = 0

    def externals(self) -> list[Any]:
        files = list(self.env.files.values())
        return list(getattr(files[0], "externals", [])) if files else []

    def go_name(self, path_id: int) -> str | None:
        return self.go_name_by_pid.get(path_id)

    def parent_go(self, go_pid: int) -> int:
        father_tr = self.go_parent_tr.get(go_pid, 0)
        return self.go_by_tr.get(father_tr, 0)

    def material_name(self, go_pid: int) -> str | None:
        name = self.go_name(go_pid)
        if name is None:
            return None
        if name == "i":
            parent = self.parent_go(go_pid)
            return self.go_name(parent)
        return name

    def first_sprite(self, go_pid: int) -> tuple[int, Any, int] | None:
        """Return (sprite_path_id, sprite_data, texture_path_id) for the first renderer."""
        seen: set[int] = set()

        def walk(pid: int):
            if pid in seen or pid not in self.go_by_pid:
                return None
            seen.add(pid)
            go = self.go_by_pid[pid]
            for comp in go.m_Component:
                if comp.component.type.name != "SpriteRenderer":
                    continue
                try:
                    sr = comp.component.read()
                    sprite_ptr = getattr(sr, "m_Sprite", None)
                    if sprite_ptr is None or not getattr(sprite_ptr, "path_id", 0):
                        continue
                    sprite = sprite_ptr.read()
                    texture_ptr = getattr(getattr(sprite, "m_RD", None), "texture", None)
                    texture_pid = int(getattr(texture_ptr, "path_id", 0) or 0)
                    return int(sprite_ptr.path_id), sprite, texture_pid
                except Exception:
                    continue
            tr_pid = self.go_tr.get(pid, 0)
            tr = self.tr_by_pid.get(tr_pid)
            if tr is not None:
                for child in getattr(tr, "m_Children", []) or []:
                    child_go = self.go_by_tr.get(int(child.path_id), 0)
                    if child_go:
                        found = walk(child_go)
                        if found is not None:
                            return found
            return None

        return walk(go_pid)


class Resolver:
    """Resolve material GameObject names from PPtrs, including external files."""

    def __init__(self):
        self.views: dict[str, AssetView] = {}

    def view(self, path: Path) -> AssetView:
        key = str(path.resolve())
        if key not in self.views:
            self.views[key] = AssetView(path)
        return self.views[key]

    def external_path(self, own_view: AssetView, file_id: int) -> Path | None:
        if file_id == 0:
            return own_view.path
        externals = own_view.externals()
        index = file_id - 1
        if index < 0 or index >= len(externals):
            return None
        ext = externals[index]
        path = getattr(ext, "path", None) or str(ext)
        if not path:
            return None
        candidate = Path(path)
        if not candidate.is_absolute():
            candidate = DATA / path
        return candidate if candidate.exists() else None

    def resolve(self, own_view: AssetView, file_id: int, path_id: int) -> str | None:
        path = self.external_path(own_view, file_id)
        if path is None:
            return None
        try:
            target = self.view(path)
        except Exception:
            return None
        return target.material_name(path_id)


# ---------------------------------------------------------------------------
# Odin / Unity serialization helpers
# ---------------------------------------------------------------------------


def parse_serialization_data(raw: bytes, header: str) -> tuple[bytes, list[tuple[int, int]]]:
    """Return the Odin byte blob and SerializationData.ReferencedUnityObjects."""
    if header == "monobehaviour":
        offset = 12 + 4 + 12  # m_GameObject PPtr, m_Enabled, m_Script PPtr
    elif header == "scriptableobject":
        offset = 12 + 12  # m_GameObject PPtr, m_Script PPtr
    else:
        raise ValueError(header)

    name_len = struct.unpack_from("<i", raw, offset)[0]
    offset = align4(offset + 4 + max(0, name_len))
    serialized_format = struct.unpack_from("<i", raw, offset)[0]
    if serialized_format != 0:
        raise ValueError(f"unsupported SerializationData format {serialized_format}")
    offset += 4
    blob_len = struct.unpack_from("<i", raw, offset)[0]
    offset += 4
    blob = raw[offset : offset + blob_len]
    offset = align4(offset + blob_len)
    ref_count = struct.unpack_from("<i", raw, offset)[0]
    offset += 4
    refs: list[tuple[int, int]] = []
    for _ in range(ref_count):
        file_id, path_id = struct.unpack_from("<iq", raw, offset)
        refs.append((int(file_id), int(path_id)))
        offset += 12
    return blob, refs


def odin_pairs(blob: bytes, field_name: str, end_field: str | None = None) -> list[tuple[int, int, bytes]]:
    """Return (key, value_offset, value_bytes) for a serialized Odin dictionary."""
    start = blob.find(field_name.encode("utf-16le"))
    if start < 0:
        raise ValueError(f"field {field_name!r} not found")
    end = blob.find(end_field.encode("utf-16le")) if end_field else len(blob)
    if end < 0:
        end = len(blob)
    if end < start:
        end = len(blob)

    parts: list[tuple[str, int, int]] = []
    for match in re.finditer(rb"\$\x00([kv])\x00", blob[start:end]):
        absolute = start + match.start()
        kind = match.group(1).decode("ascii")
        value = struct.unpack_from("<i", blob, absolute + 4)[0]
        parts.append((kind, absolute, int(value)))

    entries: list[tuple[int, int, bytes]] = []
    index = 0
    while index + 1 < len(parts):
        if parts[index][0] != "k" or parts[index + 1][0] != "v":
            index += 1
            continue
        key = parts[index][2]
        value_offset = parts[index + 1][1] + 4
        next_key = parts[index + 2][1] if index + 2 < len(parts) else end
        entries.append((key, value_offset, blob[value_offset:next_key]))
        index += 2
    return entries


def odin_float_dict(blob: bytes, field_name: str, end_field: str | None = None) -> dict[int, float]:
    result: dict[int, float] = {}
    for key, value_offset, _ in odin_pairs(blob, field_name, end_field):
        result[key] = float(struct.unpack_from("<f", blob, value_offset)[0])
    return result


def odin_int_dict(blob: bytes, field_name: str) -> dict[int, int]:
    result: dict[int, int] = {}
    for key, value_offset, _ in odin_pairs(blob, field_name):
        result[key] = int(struct.unpack_from("<i", blob, value_offset)[0])
    return result


def odin_array_dict(blob: bytes, field_name: str, ref_count: int) -> dict[int, list[int]]:
    """Parse Dictionary<enum, Object[]>; values are indices into ReferencedUnityObjects."""
    result: dict[int, list[int]] = {}
    for key, _, value in odin_pairs(blob, field_name):
        indices: list[int] = []
        cursor = 0
        while cursor < len(value) - 4:
            if value[cursor] != 0x0C:
                cursor += 1
                continue
            index = struct.unpack_from("<i", value, cursor + 1)[0]
            if 0 <= index < ref_count:
                indices.append(index)
                cursor += 5
            else:
                cursor += 1
        if not indices:
            raise ValueError(f"no object references in array for key {key}")
        result[key] = indices
    return result


def parse_default_material_asset(path: Path, resolver: Resolver) -> dict[str, Any]:
    env = UnityPy.load(str(path))
    view = resolver.view(path)
    mb = next(o for o in env.objects if o.type.name == "MonoBehaviour")
    raw = mb.get_raw_data()
    blob, refs = parse_serialization_data(raw, "monobehaviour")

    # The final two Unity-serialized int[6] fields are baseMaterialNumber and
    # extraMaterialNumber. They follow SerializationData in field order.
    tail = raw[-56:]
    if len(tail) != 56:
        raise ValueError("material_config: truncated base/extra material arrays")
    base_len, = struct.unpack_from("<i", tail, 0)
    extra_len, = struct.unpack_from("<i", tail, 28)
    if base_len != 6 or extra_len != 6:
        raise ValueError(f"material_config: unexpected array lengths {base_len}/{extra_len}")
    base_numbers = list(struct.unpack_from("<6i", tail, 4))
    extra_numbers = list(struct.unpack_from("<6i", tail, 32))

    material_factor = odin_float_dict(blob, "meterialFactor", "extraMeterialFactor")
    extra_material_factor = odin_float_dict(blob, "extraMeterialFactor", "defaultMaterial")
    default_indices = odin_array_dict(blob, "defaultMaterial", len(refs))

    default_materials: dict[int, list[str]] = {}
    for weapon_type, indices in default_indices.items():
        names: list[str] = []
        for index in indices:
            file_id, path_id = refs[index]
            name = resolver.resolve(view, file_id, path_id)
            if not name:
                raise ValueError(
                    f"material_config: cannot resolve defaultMaterial PPtr index {index} "
                    f"(fileID={file_id}, pathID={path_id})"
                )
            names.append(name)
        default_materials[int(weapon_type)] = names

    return {
        "path": path,
        "base_material_number": base_numbers,
        "extra_material_number": extra_numbers,
        "material_factor": material_factor,
        "extra_material_factor": extra_material_factor,
        "default_materials": default_materials,
        "referenced_material_names": [
            resolver.resolve(view, file_id, path_id) for file_id, path_id in refs
        ],
    }


def parse_custom_materials(raw: bytes, resolver: Resolver, own_view: AssetView) -> tuple[int, dict[str, int]]:
    blob, refs = parse_serialization_data(raw, "monobehaviour")
    entries = odin_pairs(blob, "materials")
    resolved_refs: list[str] = []
    for file_id, path_id in refs:
        name = resolver.resolve(own_view, file_id, path_id)
        if not name:
            raise ValueError(
                f"custom material PPtr unresolved (fileID={file_id}, pathID={path_id})"
            )
        resolved_refs.append(name)

    counts: dict[str, int] = {}
    for key, value_offset, _ in entries:
        if key < 0 or key >= len(resolved_refs):
            raise ValueError(f"custom material index {key} outside {len(resolved_refs)} refs")
        count = int(struct.unpack_from("<i", blob, value_offset)[0])
        name = resolved_refs[key]
        counts[name] = counts.get(name, 0) + count
    return 0, counts  # isAddtional is read through the typetree in the caller


# ---------------------------------------------------------------------------
# ItemInfo and material prefabs
# ---------------------------------------------------------------------------


def parse_localization_terms() -> dict[str, str]:
    """Recover the first (English) locale from the I2 LanguageSourceAsset.

    The asset is a serialized LanguageSourceAsset, not a TextAsset. Terms are
    stored as aligned length-prefixed strings followed by 13 locale strings;
    this is the same reproducible scan used by
    ``tools/asset/extract_localization.py``.
    """
    data = LOCALIZATION_FILE.read_bytes()
    terms: dict[str, str] = {}
    locale_count = 13
    for match in re.finditer(rb"[A-Za-z][A-Za-z0-9_/\.-]{2,80}", data):
        start = match.start()
        if start < 4:
            continue
        length = struct.unpack_from("<I", data, start - 4)[0]
        if length != len(match.group()):
            continue
        for offset in range(0, 17):
            pos = match.end() + offset
            if pos + 4 > len(data):
                break
            count = struct.unpack_from("<I", data, pos)[0]
            if count != locale_count:
                continue
            cursor = pos + 4
            values: list[str] = []
            valid = True
            for _ in range(count):
                if cursor + 4 > len(data):
                    valid = False
                    break
                size = struct.unpack_from("<I", data, cursor)[0]
                cursor += 4
                if size > 1024 or cursor + size > len(data):
                    valid = False
                    break
                try:
                    values.append(data[cursor:cursor + size].decode("utf-8"))
                except UnicodeDecodeError:
                    valid = False
                    break
                cursor += size
                cursor = align4(cursor)
            if valid and values:
                terms.setdefault(
                    match.group().decode("utf-8", "replace"), values[0]
                )
                break
    return terms


def parse_item_info() -> dict[str, dict[str, Any]]:
    env = UnityPy.load(str(ITEM_INFO_FILE))
    for obj in env.objects:
        if obj.type.name != "TextAsset":
            continue
        data = obj.read()
        if data.m_Name != "ItemInfo":
            continue
        parsed = json.loads(data.m_Script)
        return {row["name"]: row for row in parsed.get("materials", [])}
    raise ValueError(f"ItemInfo TextAsset not found in {ITEM_INFO_FILE}")


def item_component(go: Any) -> tuple[str, Any] | None:
    for comp in go.m_Component:
        if comp.component.type.name != "MonoBehaviour":
            continue
        try:
            data = comp.component.read()
            script = getattr(data, "m_Script", None)
            script_name = (
                script.read().m_ClassName
                if script is not None and getattr(script, "path_id", 0)
                else ""
            )
        except Exception:
            continue
        if script_name in ("ItemPickable", "ItemSkillFragment", "ItemSkinFragment"):
            return script_name, data
    return None


def collect_materials(common_view: AssetView, item_info: dict[str, dict[str, Any]], localization: dict[str, str]) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    materials: list[dict[str, Any]] = []
    excluded: list[dict[str, Any]] = []
    seen: set[str] = set()

    for go_pid, go in sorted(common_view.go_by_pid.items(), key=lambda item: item[1].m_Name):
        name = go.m_Name
        if not name.startswith(MATERIAL_PREFIX):
            continue
        if name in seen:
            continue
        component = item_component(go)
        if component is None:
            if name in UI_TEMPLATE_NAMES:
                excluded.append(
                    {
                        "id": name,
                        "prefab_path_id": str(go_pid),
                        "reason": "UI template without ItemPickable/ItemSkillFragment/ItemSkinFragment",
                    }
                )
            continue
        script_name, data = component
        seen.add(name)
        sprite = common_view.first_sprite(go_pid)
        info = item_info.get(name, {})
        cn_name = getattr(data, "cnName", "") or ""
        item_type = getattr(data, "itemType", None)
        item_value = getattr(data, "item_value", None)
        item_name = getattr(data, "item_name", "") or ""
        en_name = localization.get(name)
        en_term = name
        if en_name is None and item_name:
            # material_gem has item_name A_Gem; use that exact localization
            # term rather than deriving a name from display strings.
            en_name = localization.get(item_name)
            en_term = item_name
        if en_name is not None:
            display_source = f"localization:{en_term}"
        elif cn_name:
            display_source = "ItemPickable.cnName only"
        else:
            display_source = "unavailable"
        entry = {
            "id": name,
            "display_name_en": en_name,
            "display_name_zh": cn_name or None,
            "display_name_source": display_source,
            "price": info.get("price"),
            "item_type": int(item_type) if item_type is not None else None,
            "sprite_id": None,
            "sprite_rect": None,
            "atlas_path_id": None,
            "prefab_path_id": str(go_pid),
            "source_bundle": "common",
            "confidence": "CONFIRMED",
            "item_value": int(item_value) if item_value is not None else None,
            "item_name": item_name,
            "component": script_name,
            "sprite_name": None,
            "atlas_name": None,
        }
        if sprite is not None:
            sprite_pid, sprite_data, texture_pid = sprite
            rect = sprite_data.m_Rect
            entry["sprite_id"] = str(sprite_pid)
            entry["sprite_name"] = sprite_data.m_Name
            entry["sprite_rect"] = [
                float(rect.x),
                float(rect.y),
                float(rect.width),
                float(rect.height),
            ]
            entry["atlas_path_id"] = str(texture_pid) if texture_pid else None
            if texture_pid:
                try:
                    entry["atlas_name"] = common_view.object_by_pid[texture_pid].read().m_Name
                except Exception:
                    entry["atlas_name"] = None
        else:
            entry["confidence"] = "PARTIAL"
        materials.append(entry)

    materials.sort(key=lambda item: item["id"])
    excluded.sort(key=lambda item: (item["id"], item["prefab_path_id"]))
    return materials, excluded


# ---------------------------------------------------------------------------
# Weapons
# ---------------------------------------------------------------------------


def weapon_sort_key(weapon_id: str) -> tuple[int, str]:
    match = re.fullmatch(r"weapon_(\d+)", weapon_id)
    return (int(match.group(1)), weapon_id) if match else (10**9, weapon_id)


def sorted_materials(counts: dict[str, int]) -> list[dict[str, Any]]:
    return [
        {"id": material_id, "count": int(count)}
        for material_id, count in sorted(counts.items())
        if count > 0
    ]


def default_recipe(weapon: dict[str, Any], config: dict[str, Any]) -> dict[str, int]:
    weapon_type = int(weapon["weapon_type"])
    item_level = int(weapon["item_level"])
    item_value = int(weapon["item_value"])
    arrays = config["default_materials"]
    if weapon_type not in arrays:
        raise ValueError(f"no defaultMaterial entry for weapon_type={weapon_type}")
    if item_level < 0 or item_level >= len(config["base_material_number"]):
        raise ValueError(f"item_level {item_level} outside baseMaterialNumber")
    material_list = arrays[weapon_type]
    if not material_list:
        raise ValueError(f"empty defaultMaterial array for weapon_type={weapon_type}")
    factor = config["material_factor"].get(weapon_type)
    if factor is None:
        raise ValueError(f"no meterialFactor entry for weapon_type={weapon_type}")
    base = config["base_material_number"][item_level]
    per = math.ceil((base * float(factor) + round(item_value / 10.0)) / len(material_list))
    counts: dict[str, int] = {}
    for material_name in material_list:
        counts[material_name] = counts.get(material_name, 0) + per
    return counts


def explicit_recipe(component: Any, resolver: Resolver, own_view: AssetView) -> dict[str, int]:
    material_factor = float(getattr(component, "materialFactor", 1.0))
    material_ptrs = list(getattr(component, "materials", []) or [])
    extra_ptrs = list(getattr(component, "extraMaterials", []) or [])
    # WeaponMaterial.GetMaterials uses DefaultWeaponMaterial.defaultMaterial
    # when the serialized array is empty. The caller resolves that fallback.
    names: list[str] = []
    for ptr in material_ptrs:
        name = resolver.resolve(own_view, int(ptr.file_id), int(ptr.path_id))
        if not name:
            raise ValueError(f"unresolved materials PPtr {ptr.file_id}/{ptr.path_id}")
        names.append(name)
    extra_names: list[str] = []
    for ptr in extra_ptrs:
        name = resolver.resolve(own_view, int(ptr.file_id), int(ptr.path_id))
        if not name:
            raise ValueError(f"unresolved extraMaterials PPtr {ptr.file_id}/{ptr.path_id}")
        extra_names.append(name)

    # Formula is applied by the caller because it needs item fields.
    return {
        "material_factor": material_factor,
        "materials": names,
        "extra_materials": extra_names,
    }


def build_weapon_data() -> tuple[dict[str, Any], list[dict[str, Any]]]:
    catalogue = json.loads(WEAPON_CATALOGUE.read_text(encoding="utf-8"))
    by_id = {entry["id"]: entry for entry in catalogue["weapons"]}
    forgeable = [
        by_id[weapon_id]
        for weapon_id in sorted(by_id, key=weapon_sort_key)
        if by_id[weapon_id].get("forgeable")
    ]

    resolver = Resolver()
    common_view = resolver.view(COMMON)
    config = parse_default_material_asset(MATERIAL_CONFIG_FILE, resolver)

    explicit: dict[str, Any] = {}
    explicit_errors: dict[str, str] = {}
    custom: dict[str, dict[str, Any]] = {}
    custom_errors: dict[str, str] = {}

    for obj in common_view.objects:
        if obj.type.name != "MonoBehaviour":
            continue
        try:
            data = obj.read()
            script = getattr(data, "m_Script", None)
            script_name = (
                script.read().m_ClassName
                if script is not None and getattr(script, "path_id", 0)
                else ""
            )
        except Exception:
            continue
        if script_name not in ("WeaponMaterial", "CustomWeaponMaterial"):
            continue
        try:
            go_name = data.m_GameObject.read().m_Name
        except Exception:
            continue
        if not re.fullmatch(r"weapon_\d+", go_name or ""):
            continue
        if script_name == "WeaponMaterial":
            try:
                candidate = explicit_recipe(data, resolver, common_view)
                old = explicit.get(go_name)
                if old is None or (
                    not old["materials"] and candidate["materials"]
                ):
                    explicit[go_name] = candidate
                elif old["materials"] and candidate["materials"]:
                    # Deterministic duplicate resolution: lowest path id wins.
                    if obj.path_id < old["component_path_id"]:
                        candidate["component_path_id"] = obj.path_id
                        explicit[go_name] = candidate
                if "component_path_id" not in explicit[go_name]:
                    explicit[go_name]["component_path_id"] = obj.path_id
            except Exception as exc:
                explicit_errors[go_name] = str(exc)
        else:
            try:
                is_additive = bool(int(getattr(data, "isAddtional", 0)))
                _, counts = parse_custom_materials(obj.get_raw_data(), resolver, common_view)
                old = custom.get(go_name)
                if old is None or obj.path_id < old["component_path_id"]:
                    custom[go_name] = {
                        "is_additive": is_additive,
                        "materials": counts,
                        "component_path_id": obj.path_id,
                    }
            except Exception as exc:
                custom_errors[go_name] = str(exc)

    result_weapons: list[dict[str, Any]] = []
    unrecoverable: list[dict[str, Any]] = []
    source_counts = {
        "WeaponMaterial": 0,
        "CustomWeaponMaterial": 0,
        "CustomWeaponMaterialAdditive": 0,
        "DefaultWeaponMaterial": 0,
        "unrecoverable": 0,
    }

    for weapon in forgeable:
        weapon_id = weapon["id"]
        name = weapon.get("display_name") or weapon_id
        item_level = int(weapon.get("item_level", 0))
        weapon_type = int(weapon.get("weapon_type", 0))
        item_value = int(weapon.get("item_value", 0))
        base_entry = {
            "weapon_id": weapon_id,
            "name": name,
            "materials": [],
            "source": "unrecoverable",
            "item_level": item_level,
            "weapon_type": weapon_type,
            "item_value": item_value,
        }

        if weapon_id in explicit_errors:
            base_entry["reason"] = f"WeaponMaterial decode failed: {explicit_errors[weapon_id]}"
            unrecoverable.append(base_entry)
            source_counts["unrecoverable"] += 1
            continue
        if weapon_id in custom_errors:
            base_entry["reason"] = f"CustomWeaponMaterial decode failed: {custom_errors[weapon_id]}"
            unrecoverable.append(base_entry)
            source_counts["unrecoverable"] += 1
            continue

        try:
            if weapon_id in explicit:
                data = explicit[weapon_id]
                materials = data["materials"]
                extras = data["extra_materials"]
                used_default_fallback = False
                if not materials:
                    materials = config["default_materials"].get(weapon_type, [])
                    used_default_fallback = True
                if not materials:
                    raise ValueError(
                        "empty WeaponMaterial.materials and no DefaultWeaponMaterial.defaultMaterial"
                    )
                base = config["base_material_number"][item_level]
                factor = config["material_factor"].get(weapon_type)
                if factor is None:
                    raise ValueError(f"no meterialFactor for type {weapon_type}")
                per = max(
                    1,
                    int(
                        float(data["material_factor"])
                        * math.ceil(
                            (base * float(factor) + round(item_value / 10.0))
                            / len(materials)
                        )
                    ),
                )
                counts: dict[str, int] = {}
                for material_name in materials:
                    counts[material_name] = counts.get(material_name, 0) + per
                if extras:
                    extra_base = config["extra_material_number"][item_level]
                    extra_factor = config["extra_material_factor"].get(weapon_type)
                    if extra_factor is None:
                        raise ValueError(f"no extraMeterialFactor for type {weapon_type}")
                    extra_count = max(
                        1,
                        math.ceil(
                            float(extra_factor) * extra_base / len(extras)
                        ),
                    )
                    for material_name in extras:
                        counts[material_name] = counts.get(material_name, 0) + extra_count
                base_entry["source"] = "WeaponMaterial"
                if used_default_fallback:
                    base_entry["fallback"] = "DefaultWeaponMaterial.defaultMaterial"
                source_counts["WeaponMaterial"] += 1
            elif weapon_id in custom:
                data = custom[weapon_id]
                if data["is_additive"]:
                    counts = default_recipe(weapon, config)
                    for material_name, count in data["materials"].items():
                        counts[material_name] = counts.get(material_name, 0) + count
                    base_entry["source"] = "CustomWeaponMaterialAdditive"
                    source_counts["CustomWeaponMaterialAdditive"] += 1
                else:
                    counts = dict(data["materials"])
                    base_entry["source"] = "CustomWeaponMaterial"
                    source_counts["CustomWeaponMaterial"] += 1
            else:
                counts = default_recipe(weapon, config)
                base_entry["source"] = "DefaultWeaponMaterial"
                source_counts["DefaultWeaponMaterial"] += 1
            base_entry["materials"] = sorted_materials(counts)
            if not base_entry["materials"]:
                raise ValueError("formula produced an empty material list")
            result_weapons.append(base_entry)
        except Exception as exc:
            base_entry["reason"] = str(exc)
            unrecoverable.append(base_entry)
            source_counts["unrecoverable"] += 1

    result = {
        "schema_version": 1,
        "source": {
            "common_bundle": str(COMMON.relative_to(ROOT)),
            "material_config": str(MATERIAL_CONFIG_FILE.relative_to(ROOT)),
            "weapon_catalogue": str(WEAPON_CATALOGUE.relative_to(ROOT)),
            "unitypy_version": getattr(UnityPy, "__version__", "unknown"),
            "common_sha256": sha256(COMMON),
            "material_config_sha256": sha256(MATERIAL_CONFIG_FILE),
            "weapon_catalogue_sha256": sha256(WEAPON_CATALOGUE),
        },
        "formula": {
            "explicit": "per=max(1,int(materialFactor*ceil((baseMaterialNumber[item_level]*meterialFactor[weapon_type]+round(item_value/10))/len(materials))))",
            "default": "per=ceil((baseMaterialNumber[item_level]*meterialFactor[weapon_type]+round(item_value/10))/len(defaultMaterial[weapon_type]))",
            "custom_additive": "default recipe + CustomWeaponMaterial dictionary counts",
        },
        "default_material_config": {
            "base_material_number": config["base_material_number"],
            "extra_material_number": config["extra_material_number"],
            "meterial_factor": {str(k): v for k, v in sorted(config["material_factor"].items())},
            "extra_meterial_factor": {
                str(k): v for k, v in sorted(config["extra_material_factor"].items())
            },
            "default_material": {
                str(k): v for k, v in sorted(config["default_materials"].items())
            },
            "referenced_material_names": config["referenced_material_names"],
        },
        "forgeable_count": len(forgeable),
        "recovered_count": len(result_weapons),
        "unrecoverable_count": len(unrecoverable),
        "weapon_material_default_fallback": sum(
            1 for weapon in result_weapons if weapon.get("fallback")
        ),
        "totals": {
            "forgeable": len(forgeable),
            "explicit": source_counts["WeaponMaterial"],
            "custom": source_counts["CustomWeaponMaterial"],
            "additive": source_counts["CustomWeaponMaterialAdditive"],
            "default": source_counts["DefaultWeaponMaterial"],
            "unrecoverable": source_counts["unrecoverable"],
        },
        "source_counts": source_counts,
        "weapons": result_weapons,
        "unrecoverable": unrecoverable,
    }
    return result, unrecoverable


# ---------------------------------------------------------------------------
# Markdown writers
# ---------------------------------------------------------------------------


def material_markdown(materials: list[dict[str, Any]], excluded: list[dict[str, Any]], item_info: dict[str, dict[str, Any]], summary: dict[str, Any]) -> str:
    lines: list[str] = []
    lines.append("# Workshop materials")
    lines.append("")
    lines.append(
        "Generated by `tools/asset/extract_workshop_data.py` from the local "
        "Soul Knight 2.6.8 reference tree. No values are hand-authored."
    )
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    lines.append(f"- material item prefabs: **{len(materials)}**")
    lines.append(f"- ItemInfo material rows: **{len(item_info)}**")
    lines.append(f"- excluded non-item `material_*` UI templates: **{len(excluded)}**")
    lines.append(f"- English display names recovered: **{summary['english_name_count']}**")
    lines.append(f"- Chinese-only display names: **{summary['zh_only_count']}**")
    lines.append(f"- neither EN nor ZH display name: **{summary['unavailable_display_name_count']}**")
    lines.append("")
    lines.append(
        "`display_name_zh` is the serialized `ItemPickable.cnName` (or the "
        "corresponding derived item component). `display_name_en` is taken only "
        "from an exact localization term matching the material id, except "
        "`material_gem`, whose serialized `item_name` is `A_Gem` and whose "
        "localization term is `Gem`. Terms with no exact match are recorded as "
        "`null` rather than translated or guessed. The skill-fragment prefabs "
        "have an empty `cnName`, so their Chinese display name is also `null`."
    )
    lines.append("")
    lines.append("## Materials")
    lines.append("")
    lines.append("| id | zh name | en name | price | item type | sprite | atlas | prefab pathID | confidence |")
    lines.append("|---|---|---|---:|---:|---|---|---|---|")
    for item in materials:
        lines.append(
            "| `{id}` | {zh} | {en} | {price} | {item_type} | `{sprite}` {rect} | `{atlas}` | `{prefab}` | {confidence} |".format(
                id=item["id"],
                zh=item["display_name_zh"] or "—",
                en=item["display_name_en"] or "—",
                price=item["price"] if item["price"] is not None else "—",
                item_type=item["item_type"] if item["item_type"] is not None else "—",
                sprite=item["sprite_name"] or "—",
                rect=item["sprite_rect"] or "",
                atlas=item["atlas_path_id"] or "—",
                prefab=item["prefab_path_id"],
                confidence=item["confidence"],
            )
        )
    lines.append("")
    lines.append("## Excluded `material_*` prefabs")
    lines.append("")
    lines.append(
        "These are UI templates, not material item prefabs. They have no "
        "`ItemPickable` data, item sprite, or ItemInfo price and are therefore "
        "not included in `materials.json`."
    )
    lines.append("")
    for item in excluded:
        lines.append(f"- `{item['id']}` pathID `{item['prefab_path_id']}` — {item['reason']}")
    lines.append("")
    lines.append("## `ItemMaterialChest` / workshop material chest")
    lines.append("")
    lines.append(
        "Scene object: `room/objects/static/research/chest_material` "
        "(GameObject pathID `487`, world approximately `(48.93, 0.81)`), "
        "component `ItemMaterialChest` (script pathID `1243`)."
    )
    lines.append("")
    lines.append(
        "- `ItemMaterialChest` has **no loot table, count table, drop list, or "
        "ItemInfo reference**. Its declared fields are the inherited `RGItem` "
        "fields plus only `audio_clip` (+0x78)."
    )
    lines.append(
        "- The prefab uses Animator controller pathID `714` named `chest`, with "
        "clips `chest_open`/`chest_close` and decorative children "
        "`box03`, `box04`, `box05`, `box06`, `tiles_12`, `shadow`. These are "
        "presentation objects only; no item/count payload is serialized."
    )
    lines.append(
        "- Behavior (RVA `ItemMaterialChest.HoldingTicket` `0xF66B98` / "
        "`OnItemTriggerSuccess` `0xF66C20`): if the held weapon key starts with "
        "`weapon_`, the chest drops that weapon and calls "
        "`ItemData.AddTokenTicket(<suffix after \"weapon_\">, 1)`. Otherwise it "
        "sets Animator bool `OPEN`, loads/instantiates `window_chest`, gets "
        "`UIMaterialChest`, and calls `UIMaterialChest.ShowWindow` "
        "(`0xCCAB34`). The only count in this path is the literal `1`."
    )
    lines.append(
        "- `ItemInfo.json` contains the 36-row material catalog "
        "(`name`, `targetObject: null`, `price`, `isHidden`). `UIMaterialChest` "
        "uses `ItemData.materials` (the player's inventory counts) and the "
        "ItemInfo catalog to display owned materials; it does not define chest "
        "loot or per-open counts."
    )
    lines.append(
        "- `RGChestMaterial` exists as a separate reward-chest class, but the "
        "HeroRoom object is `ItemMaterialChest`, not `RGChestMaterial`."
    )
    lines.append("")
    lines.append("**Conclusion: no recoverable loot/count fields exist on this chest; "
                 "ItemInfo provides only a display/price catalog, not a loot table.**")
    lines.append("")
    return "\n".join(lines)


def weapon_markdown(data: dict[str, Any]) -> str:
    counts = data["source_counts"]
    lines: list[str] = []
    lines.append("# Workshop forge recipes")
    lines.append("")
    lines.append(
        "Generated by `tools/asset/extract_workshop_data.py` from the local "
        "Soul Knight 2.6.8 reference tree."
    )
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    lines.append(f"- forgeable weapons: **{data['forgeable_count']}**")
    lines.append(f"- recovered cost rows: **{data['recovered_count']}**")
    lines.append(f"- explicit `WeaponMaterial`: **{counts['WeaponMaterial']}**")
    lines.append(
        "  - of which empty `WeaponMaterial.materials` arrays use the in-component "
        f"`DefaultWeaponMaterial.defaultMaterial` fallback: **{data['weapon_material_default_fallback']}**"
    )
    lines.append(f"- `CustomWeaponMaterial`: **{counts['CustomWeaponMaterial']}**")
    lines.append(f"- `CustomWeaponMaterialAdditive`: **{counts['CustomWeaponMaterialAdditive']}**")
    lines.append(f"- default fallback: **{counts['DefaultWeaponMaterial']}**")
    lines.append(f"- unrecoverable: **{counts['unrecoverable']}**")
    lines.append("")
    lines.append(
        "Precedence follows `RGWeapon.GetMaterials` (RVA `0xF8F8E8`): "
        "`WeaponMaterial` > `CustomWeaponMaterial` > "
        "`DefaultWeaponMaterial`. Additive custom components start from the "
        "default recipe and add their serialized counts, matching "
        "`CustomWeaponMaterial.GetMaterials` (`isAddtional != 0`)."
    )
    lines.append("")
    lines.append("## Recovered costs")
    lines.append("")
    lines.append("| weapon | name | source | item level | type | value | materials |")
    lines.append("|---|---|---|---:|---:|---:|---|")
    for weapon in data["weapons"]:
        cost = ", ".join(
            f"{entry['id']} ×{entry['count']}" for entry in weapon["materials"]
        )
        lines.append(
            "| `{id}` | {name} | {source} | {level} | {wtype} | {value} | {cost} |".format(
                id=weapon["weapon_id"],
                name=weapon["name"].replace("|", "\\|"),
                source=weapon["source"],
                level=weapon["item_level"],
                wtype=weapon["weapon_type"],
                value=weapon["item_value"],
                cost=cost,
            )
        )
    lines.append("")
    if data["unrecoverable"]:
        lines.append("## Unrecoverable")
        lines.append("")
        for item in data["unrecoverable"]:
            lines.append(
                f"- `{item['weapon_id']}` ({item['name']}): {item.get('reason', 'unknown')}"
            )
        lines.append("")
    else:
        lines.append("## Unrecoverable")
        lines.append("")
        lines.append("None. Every forgeable catalogue entry was decoded.")
        lines.append("")
    lines.append("## Notes")
    lines.append("")
    lines.append(
        "`WeaponMaterial` uses its serialized `materialFactor`, `materials`, and "
        "`extraMaterials` arrays. `CustomWeaponMaterial` uses its Odin "
        "`Dictionary<GameObject,int>` and `isAddtional`. The default fallback "
        "resolves `DefaultWeaponMaterial.defaultMaterial` PPtrs through "
        "`SerializationData.ReferencedUnityObjects` and the external material "
        "asset files (`material_iron`, `material_gear`, `material_battery`, "
        "`material_wood`, `material_cell`, `material_gem`)."
    )
    lines.append("")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> int:
    for path in (COMMON, ITEM_INFO_FILE, MATERIAL_CONFIG_FILE, WEAPON_CATALOGUE):
        if not path.exists():
            print(f"error: missing reference input: {path}", file=sys.stderr)
            return 2

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    item_info = parse_item_info()
    localization = parse_localization_terms()
    common_view = Resolver().view(COMMON)
    materials, excluded = collect_materials(common_view, item_info, localization)
    english_name_count = sum(1 for item in materials if item["display_name_en"])
    zh_only_count = sum(
        1
        for item in materials
        if item["display_name_zh"] and not item["display_name_en"]
    )
    unavailable_count = sum(
        1
        for item in materials
        if not item["display_name_zh"] and not item["display_name_en"]
    )

    materials_doc = {
        "schema_version": 1,
        "source": {
            "common_bundle": str(COMMON.relative_to(ROOT)),
            "item_info_textasset": str(ITEM_INFO_FILE.relative_to(ROOT)),
            "item_info_name": "ItemInfo",
            "item_info_sha256": sha256(ITEM_INFO_FILE),
            "common_sha256": sha256(COMMON),
            "localization_asset": str(LOCALIZATION_FILE.relative_to(ROOT)),
            "localization_sha256": sha256(LOCALIZATION_FILE),
            "unitypy_version": getattr(UnityPy, "__version__", "unknown"),
        },
        "display_name_note": (
            "display_name_en comes from the exact localization term matching "
            "the material id (or the ItemPickable.item_name for material_gem, "
            "which maps to A_Gem -> Gem). Where no exact term exists, only the "
            "serialized cnName is recorded; no English text is invented."
        ),
        "material_count": len(materials),
        "english_name_count": english_name_count,
        "zh_only_count": zh_only_count,
        "unavailable_display_name_count": unavailable_count,
        "materials": materials,
        "excluded_ui_templates": excluded,
    }

    weapon_doc, _ = build_weapon_data()
    weapon_doc["totals"]["material_count"] = len(materials)
    weapon_doc["material_count"] = len(materials)

    (OUT_DIR / "materials.json").write_text(
        json.dumps(materials_doc, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    (OUT_DIR / "materials.md").write_text(
        material_markdown(materials, excluded, item_info, materials_doc),
        encoding="utf-8",
    )
    (OUT_DIR / "weapon_materials.json").write_text(
        json.dumps(weapon_doc, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    (OUT_DIR / "weapon_materials.md").write_text(
        weapon_markdown(weapon_doc),
        encoding="utf-8",
    )

    print("workshop extraction complete")
    print(f"  materials: {len(materials)} (excluded UI templates: {len(excluded)})")
    print(f"  forgeable: {weapon_doc['forgeable_count']}")
    print(f"  recovered: {weapon_doc['recovered_count']}")
    print(f"  source counts: {weapon_doc['source_counts']}")
    print(f"  unrecoverable: {weapon_doc['unrecoverable_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
