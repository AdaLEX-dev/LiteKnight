#!/usr/bin/env python3
"""Recover the chest catalogue from the reference AssetBundles.

This records every GameObject carrying an RGChest*/ItemChest* component, its
serialized level/count fields, fixed object_list references, visual sprite
children and Animator clip durations. Generated pixels remain local-only.
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUNDLES = ROOT / "analysis/apktool_out/assets/AssetBundles"
OUT_JSON = ROOT / "analysis/assets/chests/chest_catalogue.json"
OUT_MD = ROOT / "analysis/assets/chests/chest_catalogue.md"

CHEST_CLASSES = {
    "RGChest", "RGChestAppoint", "RGChestBoss", "RGChestRoomReward",
    "RGChestBig", "RGChestMaterial", "RGChestBigButton",
    "ItemChestBig", "ItemChestClosable", "ItemChestBattery",
    "ChestClosableSeller", "UIMaterialChest", "ObjectRoomChest",
}


def main() -> int:
    import UnityPy

    files = [
        str(path)
        for path in BUNDLES.rglob("*")
        if path.is_file() and not path.name.endswith(".manifest")
    ]
    env = UnityPy.load(*files)
    entries = []
    for obj in env.objects:
        if obj.type.name != "GameObject":
            continue
        try:
            game_object = obj.read()
        except Exception:
            continue
        scripts = []
        visuals = []
        animator_clips = []
        for component in game_object.m_Component:
            kind = component.component.type.name
            if kind == "MonoBehaviour":
                try:
                    script = component.component.read()
                    script_asset = getattr(script, "m_Script", None)
                    class_name = (
                        script_asset.read().m_ClassName
                        if script_asset is not None and script_asset.path_id
                        else None
                    )
                except Exception:
                    continue
                if class_name not in CHEST_CLASSES:
                    continue
                fields = {}
                for name in ("level", "finalLevel", "count", "award_count",
                             "has_coin", "item_name", "item_value", "repeat",
                             "isPickable", "priceFactor", "object_list",
                             "given_objs", "boss_objs", "activateClip"):
                    if not hasattr(script, name):
                        continue
                    value = getattr(script, name)
                    if name in ("object_list", "given_objs", "boss_objs"):
                        names = []
                        for pointer in value or []:
                            try:
                                target = pointer.read()
                                names.append(str(getattr(target, "m_Name", "")))
                            except Exception:
                                pass
                        fields[name] = names
                    elif name == "activateClip":
                        try:
                            fields[name] = str(value.read().m_Name)
                        except Exception:
                            pass
                    else:
                        fields[name] = value
                scripts.append({"class": class_name, "fields": fields})
            elif kind == "Transform":
                try:
                    root = component.component.read()
                except Exception:
                    root = None
                if root is not None:
                    for child_ptr in getattr(root, "m_Children", []) or []:
                        try:
                            child = child_ptr.read()
                            child_go = child.m_GameObject.read()
                        except Exception:
                            continue
                        for child_comp in child_go.m_Component:
                            if child_comp.component.type.name == "SpriteRenderer":
                                try:
                                    renderer = child_comp.component.read()
                                    sprite = (renderer.m_Sprite.read()
                                              if renderer.m_Sprite and
                                              renderer.m_Sprite.path_id else None)
                                    visuals.append({
                                        "child": str(child_go.m_Name),
                                        "sprite": getattr(sprite, "m_Name", None),
                                        "offset": [float(child.m_LocalPosition.x),
                                                   float(child.m_LocalPosition.y)],
                                        "sorting_layer": int(renderer.m_SortingLayer),
                                        "sorting_order": int(renderer.m_SortingOrder),
                                    })
                                except Exception:
                                    pass
                            elif child_comp.component.type.name == "Animator":
                                try:
                                    animator = child_comp.component.read()
                                    controller = animator.m_Controller.read()
                                    for clip_ptr in getattr(controller,
                                                            "m_AnimationClips", []) or []:
                                        clip = clip_ptr.read()
                                        animator_clips.append({
                                            "name": str(getattr(clip, "m_Name", "")),
                                            "duration": float(
                                                clip.m_MuscleClip.m_StopTime),
                                        })
                                except Exception:
                                    pass
        if not scripts:
            continue
        entries.append({
            "id": str(getattr(game_object, "m_Name", "")),
            "path_id": str(obj.path_id),
            "source_cab": str(getattr(obj.assets_file, "name", "")),
            "classes": scripts,
            "visuals": visuals,
            "animation_clips": animator_clips,
        })
    entries.sort(key=lambda item: (item["id"], item["source_cab"]))
    classes = sorted({script["class"] for entry in entries
                      for script in entry["classes"]})
    document = {
        "schema": 1,
        "tool": "tools/asset/audit_chest_catalogue.py",
        "source": "analysis/apktool_out/assets/AssetBundles",
        "counts": {
            "chest_game_objects": len(entries),
            "distinct_classes": len(classes),
            "with_visuals": sum(1 for entry in entries if entry["visuals"]),
            "with_animation_clips": sum(
                1 for entry in entries if entry["animation_clips"]),
        },
        "classes": classes,
        "classification": {
            "CONFIRMED": "RGChest*/ItemChest* class, serialized fields, visual children and Animator clip durations",
            "UNKNOWN": "selection weights for RGChestBoss/Big/Material beyond WeaponDropInfo and fixed object_list entries",
        },
        "entries": entries,
    }
    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUT_JSON.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n",
                        encoding="utf-8")
    lines = [
        "# Reference Chest Catalogue",
        "",
        f"- chest GameObjects: **{len(entries)}**",
        f"- distinct classes: **{len(classes)}**",
        f"- with visual children: **{document['counts']['with_visuals']}**",
        f"- with Animator clips: **{document['counts']['with_animation_clips']}**",
        "",
        "## Classes",
        "",
    ]
    lines += [f"- `{name}`" for name in classes]
    lines += [
        "",
        "## Animations",
        "",
        "The common `chest` Animator uses `chest_open` (1.0 s) and `chest_close`",
        "(0.4666667 s) at 30 fps. The previous native 0.45 s lid guess was wrong.",
        "",
        "## Entries",
        "",
        "| id | class | level/count | fixed objects | clips |",
        "|---|---|---|---|---|",
    ]
    for entry in entries:
        script = entry["classes"][0]
        fields = script["fields"]
        level = fields.get("level", "")
        count = fields.get("count", fields.get("award_count", ""))
        fixed = ", ".join(fields.get("object_list", []) +
                          fields.get("given_objs", []))
        clips = ", ".join(f"{clip['name']} ({clip['duration']:.3f}s)"
                          for clip in entry["animation_clips"])
        lines.append(f"| `{entry['id']}` | `{script['class']}` | "
                     f"{level}/{count} | {fixed} | {clips} |")
    lines += [
        "",
        "Fixed `RGChestAppoint.object_list` entries recovered across chest00",
        "variants: `weapon_021`, `weapon_034`, `weapon_049`, `weapon_062`,",
        "`restore_pot_max`.",
        "",
    ]
    OUT_MD.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {OUT_JSON.relative_to(ROOT)} and {OUT_MD.relative_to(ROOT)}")
    print(f"Chest GameObjects: {len(entries)}, classes: {len(classes)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
