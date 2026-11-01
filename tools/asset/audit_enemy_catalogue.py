#!/usr/bin/env python3
"""Catalogue reference enemy prefabs and their MapManagerLevel biome pools."""
from __future__ import annotations
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
BUNDLES=ROOT/"analysis/apktool_out/assets/AssetBundles"
MAP=ROOT/"analysis/assets/map_catalogue.json"
OUT=ROOT/"analysis/assets/enemies/enemy_catalogue.json"
OUT_MD=ROOT/"analysis/assets/enemies/enemy_catalogue.md"

def script_class(m):
    s=getattr(m,"m_Script",None)
    try:return s.read().m_ClassName if s and s.path_id else None
    except Exception:return None

def main()->int:
    import UnityPy
    files=[str(p) for p in BUNDLES.rglob("*") if p.is_file() and not p.name.endswith(".manifest")]
    env=UnityPy.load(*files)
    entries={}
    for o in env.objects:
        if o.type.name!="GameObject": continue
        try:g=o.read()
        except Exception:continue
        name=str(getattr(g,"m_Name",""))
        if not name.startswith("e_"): continue
        role=None; controller=None; clips=[]
        for c in g.m_Component:
            if c.component.type.name=="MonoBehaviour":
                try:m=c.component.read()
                except Exception:continue
                cls=script_class(m)
                if hasattr(m,"max_hp") and hasattr(m,"speed"):
                    role={"max_hp":int(getattr(m,"max_hp",0)),
                          "speed":float(getattr(m,"speed",0.0)),
                          "camp":int(getattr(m,"camp",0)),
                          "critical":int(getattr(m,"critical",0))}
                if cls and ("EnemyAI" in str(cls) or hasattr(m,"enemy_id")):
                    controller={
                        "class":cls,
                        "enemy_id":str(getattr(m,"enemy_id","")),
                        "ai_level":int(getattr(m,"ai_level",0)),
                        "reward_rate":int(getattr(m,"reward_rate",0)),
                        "reward_value":list(getattr(m,"reward_value",[]) or []),
                        "shoot_cd":float(getattr(m,"shoot_cd",0.0)),
                        "damage":int(getattr(m,"damage",0)) if hasattr(m,"damage") else None,
                        "atk_range":float(getattr(m,"atk_range",0.0)) if hasattr(m,"atk_range") else None,
                    }
            elif c.component.type.name=="Animator":
                try:
                    anim=c.component.read(); ctrl=anim.m_Controller.read()
                    clips=[str(p.read().m_Name) for p in getattr(ctrl,"m_AnimationClips",[]) or []]
                except Exception:pass
        if role is None and controller is None: continue
        if name not in entries:
            entries[name]={
                "id":name,
                "path_id":str(o.path_id),
                "source_cab":o.assets_file.name,
                "role":role,
                "controller":controller,
                "clips":clips,
            }
    # biome pools
    pools={}
    if MAP.is_file():
        for level in json.loads(MAP.read_text(encoding="utf-8")).get("levels",[]):
            for info in level.get("enemy_infos",[]):
                pools.setdefault(info["enemy"],[]).append(level["level"])
    for e in entries.values():
        e["biome_levels"]=sorted(set(pools.get(e["id"],[])))
    document={
        "schema":1,
        "tool":"tools/asset/audit_enemy_catalogue.py",
        "counts":{
            "enemy_game_objects":len(entries),
            "with_role_attribute":sum(1 for e in entries.values() if e["role"]),
            "with_controller":sum(1 for e in entries.values() if e["controller"]),
            "with_biome_assignment":sum(1 for e in entries.values() if e["biome_levels"]),
        },
        "classification":{
            "CONFIRMED":"serialized e_* prefab name, RoleAttribute hp/speed/camp/critical, RGEController enemy_id/EV fields, Animator clip names, MapManagerLevel enemy_infos weights",
            "UNKNOWN":"contact damage for AI classes that do not serialize a damage field; projectile AI behavior for non-EnemyAI04 classes",
        },
        "entries":sorted(entries.values(),key=lambda e:e["id"]),
    }
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(document,indent=2)+"\n",encoding="utf-8")
    lines=["# Reference Enemy Catalogue","",f"- enemy GameObjects: **{len(entries)}**",f"- with RoleAttribute: **{document['counts']['with_role_attribute']}**",f"- with controller: **{document['counts']['with_controller']}**",f"- with biome assignment: **{document['counts']['with_biome_assignment']}**","","| id | HP | speed | AI class | biome levels | clips |","|---|---:|---:|---|---|---|"]
    for e in document["entries"]:
        role=e["role"] or {}; ctrl=e["controller"] or {}
        lines.append(f"| `{e['id']}` | {role.get('max_hp','')} | {role.get('speed','')} | {ctrl.get('class','')} | {', '.join(e['biome_levels'])} | {', '.join(e['clips'])} |")
    OUT_MD.write_text("\n".join(lines)+"\n",encoding="utf-8")
    print(f"Wrote {OUT.relative_to(ROOT)}: {len(entries)} enemies")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
