#!/usr/bin/env python3
"""Export the first-pass reference audio set from Unity AudioClips.

The exported WAV files stay local-only/gitignored. The committed manifest records
the actual clip names, durations, channels and export targets.
"""
from __future__ import annotations
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
BUNDLES=ROOT/"analysis/apktool_out/assets/AssetBundles"
OUT=ROOT/"assets/generated/audio"
META=ROOT/"analysis/assets/audio/audio_catalogue.json"

MAPPING={
    "ui_click":"fx_tapClick",
    "pause":"fx_tapClick",
    "pickup":"fx_pickup",
    "chest_open":"fx_chest_open",
    "weapon_fire":"fx_gun_1",
    "player_hit":"fx_hit",
    "enemy_hit":"fx_hit",
    "door":"fx_door",
    "transition":"fx_door",
    "bgm_hub":"bgm_room",
    "bgm_dungeon":"bgm_1Low",
}

def main()->int:
    import UnityPy
    files=[str(p) for p in BUNDLES.rglob("*") if p.is_file() and not p.name.endswith(".manifest")]
    env=UnityPy.load(*files)
    wanted={v:k for k,v in MAPPING.items()}
    found={}
    for o in env.objects:
        if o.type.name!="AudioClip": continue
        try: clip=o.read()
        except Exception: continue
        name=str(getattr(clip,"m_Name",""))
        if name not in wanted or name in found: continue
        samples=clip.samples
        if not samples: continue
        data=next(iter(samples.values()))
        found[name]={
            "source_clip":name,
            "source_cab":o.assets_file.name,
            "duration":float(clip.m_Length),
            "channels":int(clip.m_Channels),
            "frequency":int(clip.m_Frequency),
            "data":data,
        }
    OUT.mkdir(parents=True,exist_ok=True)
    entries=[]
    for logical,clip_name in MAPPING.items():
        item=found.get(clip_name)
        if item is None:
            print(f"MISSING {logical} <- {clip_name}")
            continue
        target=OUT/f"{logical}.wav"
        target.write_bytes(item["data"])
        entries.append({
            "id":logical,
            "source_clip":item["source_clip"],
            "source_cab":item["source_cab"],
            "file":target.name,
            "duration":item["duration"],
            "channels":item["channels"],
            "frequency":item["frequency"],
        })
    META.parent.mkdir(parents=True,exist_ok=True)
    document={
        "schema":1,
        "tool":"tools/asset/build_audio_assets.py",
        "classification":{
            "CONFIRMED":"actual Unity AudioClip sample bytes and serialized length/channels/frequency",
            "HYPOTHESIS":"logical event mapping for the first-pass native audio wiring",
        },
        "counts":{"logical_events":len(MAPPING),"exported":len(entries)},
        "sounds":entries,
    }
    META.write_text(json.dumps(document,indent=2)+"\n",encoding="utf-8")
    print(f"Wrote {META.relative_to(ROOT)} and {len(entries)} WAV files")
    return 0 if entries else 1

if __name__=="__main__":
    raise SystemExit(main())
