#!/usr/bin/env python3
"""Recover the player-facing weapon catalogue from the reference game data.

Sources (SoulKnight 2.6.8 APK extraction):

* ``WeaponInfo`` TextAsset: internal prefab id, Chinese title, item level,
  drop level, type and forgeability.
* Localization table: English (and other locale) names keyed by
  ``weapon/weapon_XXX``.
* Common AssetBundle weapon prefabs: serialized ``consume`` (the per-attack
  energy cost), first bullet damage/speed/crit/repel/through, item value and
  weapon type.
* Integrated native scenes: attack cadence for the four weapons currently used
  by the native world.

The output is metadata only. No reference pixels are copied.
"""
from __future__ import annotations

import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "analysis/apktool_out/assets/bin/Data"
BUNDLES = ROOT / "analysis/apktool_out/assets/AssetBundles"
LOCALIZATION = DATA / "65e9ec86e4ca50c478ad2b19c9878f51"
WEAPON_INFO = DATA / "51df2f03793f4c6438a162bc856f1420"
SCENE = ROOT / "analysis/assets/scenes/r1_1_visual.json"
OUT_JSON = ROOT / "analysis/assets/weapon_catalogue.json"
OUT_MD = ROOT / "analysis/assets/weapon_catalogue.md"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_localized_names(path: Path) -> dict[str, str]:
    """Parse the reference key/localized-string table.

    The serialized table stores a 32-bit count followed by 32-bit length +
    UTF-8 strings with NUL padding to four-byte alignment. Strings include the
    key and every locale; the first locale after the key is English.
    """
    data = path.read_bytes()
    names: dict[str, str] = {}
    for match in re.finditer(rb"weapon/weapon_(\d{3})", data):
        key = match.group(0)
        weapon_id = f"weapon_{match.group(1).decode('ascii')}"
        if weapon_id in names:
            continue
        pos = match.start() + len(key)
        count = struct.unpack_from("<I", data, pos + 7)[0]
        pos += 11
        if count < 2 or count > 32:
            continue
        values: list[str] = []
        valid = True
        for _ in range(count):
            if pos + 4 > len(data):
                valid = False
                break
            length = struct.unpack_from("<I", data, pos)[0]
            pos += 4
            if length > 512 or pos + length > len(data):
                valid = False
                break
            values.append(data[pos:pos + length].decode("utf-8", "replace"))
            pos += length
            pos = (pos + 3) & ~3
        if valid and values:
            names[weapon_id] = values[0]
    return names


def load_weapon_info(path: Path) -> dict[str, dict]:
    import UnityPy

    env = UnityPy.load(str(path))
    for obj in env.objects:
        if obj.type.name != "TextAsset":
            continue
        data = obj.read()
        if str(getattr(data, "m_Name", "")) != "WeaponInfo":
            continue
        document = json.loads(data.m_Script)
        return {
            str(item.get("name")): item
            for item in document.get("weapons", [])
            if isinstance(item, dict) and item.get("name")
        }
    raise SystemExit("WeaponInfo TextAsset not found")


def scan_weapon_prefabs() -> dict[str, dict]:
    import UnityPy

    files = [
        str(path)
        for path in BUNDLES.rglob("*")
        if path.is_file() and not path.name.endswith(".manifest")
    ]
    env = UnityPy.load(*files)
    stats: dict[str, dict] = {}
    for obj in env.objects:
        if obj.type.name != "GameObject":
            continue
        try:
            game_object = obj.read()
        except Exception:
            continue
        weapon_id = str(getattr(game_object, "m_Name", ""))
        if re.fullmatch(r"weapon_\d{3}", weapon_id) is None:
            continue
        if weapon_id in stats:
            continue
        for component in game_object.m_Component:
            if component.component.type.name != "MonoBehaviour":
                continue
            try:
                script = component.component.read()
            except Exception:
                continue
            bullets = getattr(script, "bulletsInfo", None)
            if not bullets:
                continue
            bullet = bullets[0]
            stats[weapon_id] = {
                "id": weapon_id,
                "consume": int(getattr(script, "consume", 0)),
                "damage": int(getattr(bullet, "damage", 0)),
                "projectile_speed": float(getattr(bullet, "speed", 0.0)),
                "critic": int(getattr(bullet, "critic", 0)),
                "repel": int(getattr(bullet, "repel", 0)),
                "size": float(getattr(bullet, "size", 0.0)),
                "through_count": int(getattr(bullet, "throughCount", 0)),
                "weapon_type": int(getattr(script, "weapon_type", 0)),
                "weapon_speed": float(getattr(script, "weapon_speed", 0.0)),
                "item_level": int(getattr(script, "item_level", 0)),
                "item_value": int(getattr(script, "item_value", 0)),
                "is_melee": bool(getattr(script, "is_melee", 0)),
            }
            break
    return stats


def main() -> int:
    localized = parse_localized_names(LOCALIZATION)
    info = load_weapon_info(WEAPON_INFO)
    prefab_stats = scan_weapon_prefabs()
    integrated: dict[str, dict] = {}
    if SCENE.exists():
        scene = json.loads(SCENE.read_text(encoding="utf-8"))
        for weapon in scene.get("weapons", []):
            weapon_id = str(weapon.get("id", ""))
            if not weapon_id:
                continue
            integrated[weapon_id] = {
                "damage": weapon.get("damage"),
                "energy_cost": weapon.get("energy_cost", weapon.get("consume")),
                "attack_cycle_seconds": weapon.get("attack_cycle_seconds"),
                "projectile": (weapon.get("projectile") or {}).get("id"),
                "display_name": weapon.get("display_name", weapon_id),
            }

    ids = sorted(
        {value for value in (set(localized) | set(info) | set(prefab_stats))
         if re.fullmatch(r"weapon_\d{3}", value)},
        key=lambda value: int(value.split("_")[1]))
    weapons = []
    for weapon_id in ids:
        meta = info.get(weapon_id, {})
        stat = prefab_stats.get(weapon_id, {})
        entry = {
            "id": weapon_id,
            "display_name": localized.get(weapon_id, weapon_id),
            "title_cn": meta.get("title"),
            "type": meta.get("type"),
            "level": meta.get("level"),
            "drop_level": meta.get("dropLevel"),
            "forgeable": meta.get("forgeable"),
            "tags": meta.get("tags", []),
        }
        if stat:
            entry.update({
                "energy_cost": stat["consume"],
                "damage": stat["damage"],
                "projectile_speed": stat["projectile_speed"],
                "critic": stat["critic"],
                "repel": stat["repel"],
                "projectile_size": stat["size"],
                "through_count": stat["through_count"],
                "weapon_type": stat["weapon_type"],
                "weapon_speed": stat["weapon_speed"],
                "item_level": stat["item_level"],
                "item_value": stat["item_value"],
                "is_melee": stat["is_melee"],
            })
        if weapon_id in integrated:
            entry["integrated"] = integrated[weapon_id]
        weapons.append(entry)

    document = {
        "schema": 1,
        "tool": "tools/asset/audit_weapon_catalogue.py",
        "sources": {
            "weapon_info": {
                "path": str(WEAPON_INFO.relative_to(ROOT)),
                "sha256": sha256(WEAPON_INFO),
            },
            "localization": {
                "path": str(LOCALIZATION.relative_to(ROOT)),
                "sha256": sha256(LOCALIZATION),
            },
            "weapon_bundles": "analysis/apktool_out/assets/AssetBundles",
            "integrated_scene": str(SCENE.relative_to(ROOT)),
        },
        "counts": {
            "localized": len(localized),
            "weapon_info": len(info),
            "serialized_prefabs": len(prefab_stats),
            "catalogue": len(weapons),
            "integrated": len(integrated),
        },
        "classification": {
            "CONFIRMED": "english/localized names, serialized consume, first-bullet damage and prefab metadata",
            "UNKNOWN": "full per-weapon attack cadence for weapons outside the four integrated definitions",
        },
        "weapons": weapons,
    }
    OUT_JSON.write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n",
                        encoding="utf-8")

    lines = [
        "# Reference Weapon Catalogue",
        "",
        "Recovered player-facing names and serialized combat fields from the",
        "SoulKnight 2.6.8 game data. `energy_cost` is the serialized",
        "`RGWeapon.consume`; it is the per-attack energy cost, not ammunition.",
        "Damage is the first serialized `BulletInfoInspector.damage` entry.",
        "",
        f"- localized names: **{len(localized)}**",
        f"- WeaponInfo definitions: **{len(info)}**",
        f"- serialized weapon prefabs with a gun/bullet component: **{len(prefab_stats)}**",
        f"- integrated native weapons: **{len(integrated)}**",
        "",
        "## Integrated weapons (CONFIRMED)",
        "",
        "| Internal id | Player name | Damage | Energy | Cycle (s) | Projectile |",
        "|---|---|---:|---:|---:|---|",
    ]
    for weapon_id in sorted(integrated, key=lambda value: int(value.split("_")[1])):
        stats = integrated[weapon_id]
        lines.append(
            f"| `{weapon_id}` | {stats['display_name']} | {stats['damage']} | "
            f"{stats['energy_cost']} | {stats['attack_cycle_seconds']} | "
            f"`{stats['projectile']}` |"
        )
    lines += [
        "",
        "## Catalogue excerpt",
        "",
        "| Internal id | Player name | Level | Type | Damage | Energy |",
        "|---|---|---:|---|---:|---:|",
    ]
    for entry in weapons:
        if "damage" not in entry:
            continue
        lines.append(
            f"| `{entry['id']}` | {entry['display_name']} | "
            f"{entry.get('level', '')} | {entry.get('type') or ''} | "
            f"{entry['damage']} | {entry['energy_cost']} |"
        )
    lines += [
        "",
        "## UNKNOWN / not claimed",
        "",
        "- Attack cadence is only recovered for the four integrated weapons.",
        "- Melee/non-projectile entries without a `bulletsInfo` component are",
        "  listed by name and metadata but do not claim bullet damage.",
        "- Skin/variant localization keys that do not correspond to a base",
        "  `weapon_XXX` prefab are not treated as separate weapons.",
        "",
    ]
    OUT_MD.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {OUT_JSON.relative_to(ROOT)} and {OUT_MD.relative_to(ROOT)}")
    print(f"Catalogue: {len(weapons)} names, {len(prefab_stats)} prefab stat rows, "
          f"{len(integrated)} integrated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
