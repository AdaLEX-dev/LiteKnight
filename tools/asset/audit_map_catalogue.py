#!/usr/bin/env python3
"""Catalogue MapManagerLevel generation configs and enemy pools."""
from __future__ import annotations
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
BUNDLES=ROOT/"analysis/apktool_out/assets/AssetBundles"
OUT=ROOT/"analysis/assets/map_catalogue.json"

def name_of(ptr):
    try:
        obj=ptr.read(); return str(getattr(obj,"m_Name",""))
    except Exception:
        return ""

def main():
    import UnityPy
    files=[str(p) for p in BUNDLES.rglob("*") if p.is_file() and not p.name.endswith(".manifest")]
    env=UnityPy.load(*files)
    levels={}
    for o in env.objects:
        if o.type.name!="MonoBehaviour":
            continue
        try:m=o.read()
        except Exception: continue
        if not (hasattr(m,"level") and hasattr(m,"room_start_list")):
            continue
        level=str(getattr(m,"level",""))
        if not level or level in levels:
            continue
        def list_names(value):
            result=[]
            for ptr in value or []:
                n=name_of(ptr)
                if n: result.append(n)
            return result
        enemies=[]
        for info in getattr(m,"enemy_infos",[]) or []:
            enemies.append({
                "enemy": name_of(getattr(info,"enemy",None)),
                "exEnemy": name_of(getattr(info,"exEnemy",None)),
                "weight": int(getattr(info,"weight",0)),
            })
        levels[level]={
            "level": level,
            "source_cab": o.assets_file.name,
            "path_id": str(o.path_id),
            "chest_level": int(getattr(m,"chest_level",0)),
            "map_long": int(getattr(m,"map_long",0)),
            "has_badass": bool(getattr(m,"has_badass",False)),
            "has_cage_obj": bool(getattr(m,"has_cage_obj",False)),
            "use_new_floor_pattern": bool(getattr(m,"use_new_floor_pattern",False)),
            "centerPoint": [float(getattr(m.centerPoint,"x",0.0)), float(getattr(m.centerPoint,"y",0.0))],
            "room_start_list": list_names(getattr(m,"room_start_list",[])),
            "room_end_list": list_names(getattr(m,"room_end_list",[])),
            "branch_room_list": list_names(getattr(m,"branch_room_list",[])),
            "floor_list": list_names(getattr(m,"floor_list",[])),
            "wall_list": list_names(getattr(m,"wall_list",[])),
            "obstacle_list": list_names(getattr(m,"obstacle_list",[])),
            "enemy_infos": enemies,
        }
    document={
        "schema":1,
        "tool":"tools/asset/audit_map_catalogue.py",
        "counts":{
            "levels":len(levels),
            "levels_with_enemy_pools":sum(1 for v in levels.values() if v["enemy_infos"]),
            "levels_with_start_rooms":sum(1 for v in levels.values() if v["room_start_list"]),
        },
        "classification":{
            "CONFIRMED":"serialized MapManagerLevel fields: level, chest_level, map_long, room start/end/branch lists, floor/wall/obstacle prefab names, EnemyInfo weights",
            "UNKNOWN":"runtime MapManagerLevel.CreateRoom procedural placement for unintegrated levels",
        },
        "levels":[levels[k] for k in sorted(levels)],
    }
    OUT.write_text(json.dumps(document,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print(f"Wrote {OUT.relative_to(ROOT)}: {len(levels)} levels")
    for level in sorted(levels):
        v=levels[level]
        print(level, "chest",v["chest_level"],"map_long",v["map_long"],"enemies",len(v["enemy_infos"]),"start",v["room_start_list"][:4])

if __name__=="__main__":
    raise SystemExit(main())
