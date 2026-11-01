#!/usr/bin/env python3
"""Validate committed reference catalogues without requiring UnityPy."""
from __future__ import annotations
import json
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]

def load(path):
    return json.loads((ROOT/path).read_text(encoding="utf-8"))

def main()->int:
    rooms=load("analysis/assets/room_catalogue.json")
    maps=load("analysis/assets/map_catalogue.json")
    enemies=load("analysis/assets/enemies/enemy_catalogue.json")
    weapons=load("analysis/assets/weapon_runtime_integration.json")
    audio=load("analysis/assets/audio/audio_catalogue.json")
    errors=[]
    if rooms["counts"]["discovered_pattern_rooms"] != 108:
        errors.append("expected 108 PatternRooms")
    if len(rooms["rooms"]) != 108:
        errors.append("room catalogue length != 108")
    for room in rooms["rooms"]:
        if (room.get("width"), room.get("height")) not in {
            (15,15),(15,21),(21,15),(21,21)}:
            errors.append(f"unexpected room size: {room['id']}")
        if room.get("room_type") != 1:
            errors.append(f"unexpected room type: {room['id']}")
    if len(maps["levels"]) != 18:
        errors.append("expected 18 MapManagerLevel configs")
    level_11=next((x for x in maps["levels"] if x["level"]=="1-1"),None)
    if level_11 is None:
        errors.append("missing MapManagerLevel 1-1")
    else:
        if level_11["map_long"] != 3 or level_11["chest_level"] != 1:
            errors.append("1-1 map_long/chest_level mismatch")
        if len(level_11["enemy_infos"]) < 4:
            errors.append("1-1 enemy pool too small")
    if enemies["counts"]["enemy_game_objects"] != 102:
        errors.append("expected 102 enemy prefabs")
    if enemies["counts"]["with_biome_assignment"] < 70:
        errors.append("enemy biome assignment count too low")
    counts=weapons["counts"]
    total=(counts["tier_a_fully_integrated"]+
           counts["tier_b_melee_pending"]+
           counts["tier_c_unique_or_catalogue_only"])
    if counts["catalogue_entries"] != 302 or total != 302:
        errors.append(f"weapon tier partition invalid: {counts}")
    if audio["counts"]["exported"] != 11:
        errors.append("expected 11 reference audio exports")
    if errors:
        for error in errors:
            print("ERROR:",error)
        return 1
    print("Reference catalogues: PASS")
    print(f"  rooms={len(rooms['rooms'])} maps={len(maps['levels'])} "
          f"enemies={enemies['counts']['enemy_game_objects']} "
          f"weapons={counts['catalogue_entries']} audio={audio['counts']['exported']}")
    return 0

if __name__=="__main__":
    sys.exit(main())
