#!/usr/bin/env python3
"""Recover per-character baseline weapons from GetInitWeaponName data."""
from __future__ import annotations
import json, re, struct
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
LOCALIZATION=ROOT/"analysis/apktool_out/assets/bin/Data/65e9ec86e4ca50c478ad2b19c9878f51"
OUT=ROOT/"analysis/assets/characters/character_loadouts.json"
OUT_MD=ROOT/"analysis/assets/characters/character_loadouts.md"

HEROES=[
    ("c00",0,"Knight","weapon_000",{5:"x",9:"xx"}),
    ("c01",1,"Ranger","weapon_init_ranger",{5:"x",7:"xx"}),
    ("c02",2,"Mage","weapon_init_mage",{6:"x",9:"xx"}),
    ("c03",3,"Assassin","weapon_init_assassin",{6:"x",8:"xx"}),
    ("c04",4,"Alchemist","weapon_init_alchemist",{4:"x",6:"xx"}),
    ("c05",5,"Engineer","weapon_init_engineer",{6:"x",8:"xx"}),
    ("c06",6,"Vampire","weapon_init_vampire",{6:"x",8:"xx"}),
    ("c07",7,"Paladin","weapon_init_paladin",{6:"x",9:"xx"}),
    ("c08",8,"Elves","weapon_init_elves",{5:"x",7:"xx"}),
    ("c09",9,"Werewolf","weapon_init_werewolf",{8:"x",10:"xx"}),
    ("c10",10,"Priest","weapon_init_priest",{4:"x",7:"xx"}),
    ("c11",11,"Druid","weapon_init_druid",{5:"x",7:"xx"}),
    ("c12",12,"Robot","weapon_init_robot",{3:"x",6:"xx"}),
    ("c13",13,"Viking","weapon_init_viking",{2:"x",7:"xx"}),
    ("c14",14,"Necromancer","weapon_init_necromancer",{}),
]

def parse_names()->dict[str,str]:
    data=LOCALIZATION.read_bytes()
    names={}
    for match in re.finditer(rb"weapon/weapon_(?:init_[a-z0-9_]+|\d{3})",data):
        key=match.group(0).decode("ascii")
        full=key.split("/",1)[1]
        if full in names: continue
        end=match.end()
        vals=[]
        for off in range(0,13):
            pos=end+off
            if pos+4>len(data): break
            count=struct.unpack_from("<I",data,pos)[0]
            if not (2 <= count <= 32): continue
            pos+=4; candidate=[]; ok=True
            for _ in range(count):
                if pos+4>len(data): ok=False; break
                n=struct.unpack_from("<I",data,pos)[0]; pos+=4
                if n>512 or pos+n>len(data): ok=False; break
                candidate.append(data[pos:pos+n].decode("utf-8","replace")); pos+=n; pos=(pos+3)&~3
            if ok and candidate:
                vals=candidate; break
        if vals: names[full]=vals[0]
    return names

def main()->int:
    names=parse_names()
    entries=[]
    for char_id,hero_id,name,base,suffixes in HEROES:
        variants=[base]
        for suffix in sorted(set(suffixes.values())):
            variants.append(base+suffix)
        entries.append({
            "id":char_id,
            "hero_index":hero_id,
            "hero_name":name,
            "starting_weapon":base,
            "starting_weapon_name":names.get(base,base),
            "level_gate":7,
            "skin_suffixes":{str(k):v for k,v in suffixes.items()},
            "variants":[{"id":v,"display_name":names.get(v,v)} for v in variants],
        })
    doc={
        "schema":1,
        "tool":"tools/asset/audit_character_loadouts.py",
        "evidence":{
            "CONFIRMED":"RGController.GetInitWeaponName (0x117D954): level<7 returns base weapon; Knight weapon_000; other heroes weapon_init_<lower enum name>.",
            "HIGH_CONFIDENCE":"UIHeroSkinWeaponIndex Odin blob skin->suffix mapping.",
        },
        "default_level_rule":"char_list[hero].level < 7 -> base prefab",
        "characters":entries,
    }
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(doc,indent=2)+"\n",encoding="utf-8")
    lines=["# Character Baseline Loadouts","", "| id | hero | base prefab | display name | level gate |", "|---|---|---|---|---:|"]
    for e in entries:
        lines.append(f"| `{e['id']}` | {e['hero_name']} | `{e['starting_weapon']}` | {e['starting_weapon_name']} | {e['level_gate']} |")
    OUT_MD.write_text("\n".join(lines)+"\n",encoding="utf-8")
    print(f"Wrote {OUT.relative_to(ROOT)}: {len(entries)} characters")
    for e in entries: print(e['id'],e['hero_name'],e['starting_weapon'],e['starting_weapon_name'])
    return 0

if __name__=="__main__":
    raise SystemExit(main())
