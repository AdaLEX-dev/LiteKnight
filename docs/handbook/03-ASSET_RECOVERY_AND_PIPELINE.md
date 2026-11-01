# 03 — Asset Recovery and Pipeline

## 1. Legal and ethical boundary

The reference game is Soul Knight 2.6.8 by ChillyRoom. Its APK, Unity bundles,
textures, audio, localization, and prefabs are proprietary. This repository is a
clean-room native reconstruction, but it uses local reference material for
evidence and pixel extraction.

Rules:

- `original/`, `analysis/apktool_out/`, `assets/generated/`, and extracted
  pixels/audio are **local only**.
- Never commit the APK or extracted reference data.
- Never attach reference assets to a public GitHub release.
- Committed metadata may describe reference facts, IDs, paths, and topology, but
  must not embed the reference pixels.
- Generated PNGs are gitignored. The runtime uses locally generated assets.
- The user must supply their own reference bundle to reproduce the visual mode.

Read `LEGAL.md`, `docs/legal/clean-room-boundary.md`, and
`docs/legal/reference-assets.md` before publishing anything.

## 2. Reference locations

Expected local paths:

```text
original/SoulKnight-2.6.8.apk
analysis/apktool_out/                      # extracted APK tree
analysis/apktool_out/assets/bin/Data/      # Unity serialized files
analysis/apktool_out/assets/AssetBundles/  # asset bundles
analysis/apktool_out/assets/bin/Data/level2
analysis/apktool_out/assets/bin/Data/sharedassets2.assets[.split*]
```

The APK SHA-256 recorded by the project is:

```text
8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea
```

Reference engine: Unity 2018.4.12f1, IL2CPP metadata v24.

## 3. Python environment

The asset tools use UnityPy and Pillow:

```bash
tools/asset/.venv/bin/python tools/asset/<script>.py
```

If the venv is missing:

```bash
python3 -m venv tools/asset/.venv
tools/asset/.venv/bin/pip install -r tools/asset/requirements.txt
```

Do not run asset builders with the system Python unless you know UnityPy is
installed there. The venv is the supported path.

## 4. Core builders

| Builder | Output | Purpose |
|---------|--------|---------|
| `build_asset_inventory.py` | `analysis/assets/asset_inventory.json` | Reference texture/sprite/category inventory and redistribution classification. |
| `build_visual_scene.py` | `assets/generated/visual/<room>/`, `analysis/assets/scenes/<room>.json` | Recover a dungeon room’s sprites, layout, collision, doors, themes. |
| `build_hub_scene.py` | `assets/generated/visual/hero_room/`, `analysis/assets/scenes/hero_room.json` | Recover the Living Room hub geometry, sprites, markers, collision, NPCs, stations. |
| `build_item_assets.py` | `analysis/assets/items/item_assets.json`, generated item textures | Chests, coins, materials, shop items, portals, energy, weapon drops. |
| `build_plant_assets.py` | plant metadata/sprites appended to item assets | Garden species, growth states, seed icons. |
| `build_seller_assets.py` | seller visuals | r_sell room NPC/pedestal art. |
| `build_pattern_rooms.py` | 108 PatternRoom scenes | Dungeon room templates. |
| `build_ui_scene.py`, `build_ui_font.py` | UI atlases/font | Recovered HUD/menu assets. |
| `build_audio_assets.py` | audio catalogue/assets | Recovered sound/music metadata. |
| `build_gallery_manifest.py` | texture gallery manifest | Local visual audit gallery. |
| `extract_localization.py` | `analysis/assets/localization.json` | Recovered localization terms. |
| `extract_workshop_data.py` | `analysis/assets/workshop/weapon_materials.json` | Forge recipes/material costs. |
| `merge_canonical_weapons.py` | `analysis/assets/weapon_catalogue.json` | 302-entry weapon catalogue merge. |

Run the full local preparation:

```bash
./scripts/prepare-assets.sh
```

`prepare-assets.sh` is cache-aware. It may skip a builder if its output already
exists and basic grep checks pass. If you changed a builder but the output did
not change, run that builder directly:

```bash
tools/asset/.venv/bin/python tools/asset/build_hub_scene.py
tools/asset/.venv/bin/python tools/asset/build_item_assets.py
./scripts/prepare-assets.sh
```

## 5. What gets committed vs generated

Committed:

```text
analysis/assets/scenes/*.json
analysis/assets/scenes/*_manifest.json
analysis/assets/items/item_assets.json
analysis/assets/items/item_assets_manifest.json
analysis/assets/asset_inventory.json
analysis/assets/localization.json
analysis/assets/weapon_catalogue.json
analysis/assets/workshop/weapon_materials.json
analysis/re/*.md, *.json
tools/asset/*.py
```

Not committed:

```text
original/
analysis/apktool_out/
assets/generated/visual/**/*.png
assets/generated/items/**/*.png
build/
build-debug/
build-release/
*.log
```

The generated scene JSON references texture PNG filenames, but the PNGs are
copied to `build/assets/scenes/<room>/textures/` by `prepare-assets.sh`. The
committed scene JSON is metadata/topology, not a pixel distribution.

## 6. How a room scene is built

`build_visual_scene.py` / `build_hub_scene.py` generally:

1. join split sharedassets files if needed;
2. load `level2` and the relevant shared asset files with UnityPy;
3. walk `GameObject` transforms into world positions;
4. filter inactive event/progression objects;
5. extract SpriteRenderer bounds, pivot, PPU, sorting layer/order;
6. extract texture atlases and crop sprites to PNGs;
7. convert tilemaps into floor/wall cells and solids;
8. convert small Box/Circle/Polygon colliders into AABBs;
9. emit interaction markers (NPCs, chests, stations, portals);
10. write `assets/generated/visual/<room>/scene.json`, textures, and the
    analysis scene JSON/manifest.

The hub builder additionally:

- removes placeholder signboard slots;
- keeps the functional task board;
- restores `magic_protal1` and the guardian/grave visual;
- moves the magic NPC along the NPC→grave axis;
- seats c13/c04/c08 on the table chairs;
- removes the old stair repair wall and 3x3 block;
- shortens the protruding wall cap at `(7.5, -22.5)`;
- adds furniture footprints for object-portal tables and oxygen bottles;
- adds per-table forge collision;
- creates hub interaction markers and dialogue lines from localization;
- emits NPC markers for every `characters/cXX/img/body`.

## 7. Sprite identity and the path-id collision trap

Unity path IDs are only unique inside one asset file. Two different sprites from
different files can both have path ID `1`. The early hub builder named sprites
`hub_spr_<path_id>`, which caused aliasing: the transparent `nothing` lighting
sprite for the Rogue was silently replaced by the signboard sprite `hub_spr_1`.
That is why the Rogue appeared to be missing under a signboard.

The builder now compares texture/rect/pivot when a path ID already exists and
appends a content hash for collisions:

```text
hub_spr_1                  # actual signboard/atlas cell
hub_spr_1_f37a155b53       # transparent `nothing` light sprite
hub_spr_1_167d7e2932       # another collision-disambiguated sprite
```

If you see a wrong sprite that is “almost right”, first check whether two source
sprites share a path ID. Do not add a hardcoded exception; fix the sprite ID
generation.

## 8. Finding a reference asset

Typical workflow:

```bash
# Search names in the extracted reference tree
grep -R "Object_task" -n analysis/apktool_out/assets/bin/Data 2>/dev/null | head
grep -R "magic_protal1" -n analysis/apktool_out/assets/bin/Data 2>/dev/null | head

# Search recovered metadata
grep -R "task/board" -n analysis/assets | head
grep -R "weapon_init_ranger" -n analysis/assets src | head

# Search localization
python3 - <<'PY'
import json
t=json.load(open('analysis/assets/localization.json'))['terms']
for k,v in t.items():
    if 'task' in (k+' '+str(v)).lower():
        print(k, repr(v))
PY
```

For a specific GameObject, write a small UnityPy inspection script that:

1. loads `level2` plus the shared assets files;
2. iterates GameObjects by name;
3. prints the transform hierarchy, components, `m_LocalPosition`, active flag;
4. prints SpriteRenderer sprite name/rect/pivot/layer/order;
5. prints collider size/offset/trigger;
6. prints MonoBehaviour script class and typetree when readable.

This is the fastest way to answer “what prefab is this?”. Do not guess from the
runtime scene alone.

## 9. Evidence standard

Every recovered fact should have one of these states:

- **CONFIRMED** — direct serialized field, symbol, or observed prefab.
- **RECOVERED** — derived from a specific prefab/animation/asset with a traceable
  path.
- **HYPOTHESIS** — plausible reconstruction, explicitly marked.
- **UNKNOWN** — not recoverable from the available artifacts.
- **NATIVE EXTENSION** — a LiteKnight behavior that is not claimed to match the
  reference.

Use the existing `classification`/`evidence` fields in generated metadata and
`docs/EVIDENCE_STANDARD.md`. When a user asks for “the actual prefab”, a
hypothesis is not acceptable. Either find it or document the blocker.

## 10. Localization and dialogue

`analysis/assets/localization.json` contains recovered terms. NPC dialogue must
come from it. Example terms used in the hub:

```text
adv_toturial/room_talk_1 ... room_talk_5   # explorer tutorial NPC
I_plot1 / I_plot2 / I_plot3                 # magic stone plot
Object_task / Object_quest / Object_officer
Character1_name_skin0                       # Rogue identity
```

Do not invent dialogue when a recovered term exists. If a term is missing, say
so in the marker/evidence and use a clearly marked native fallback.

## 11. Collision extraction rules

- Wall tilemaps: each wall cell becomes a 1x1 solid.
- Floor edge: thin solids (around 0.02 thick) mark the dark-void boundary. They
  are legitimate and should not be removed as “invisible walls”.
- Composite colliders: skipped because their AABB often seals an entire room.
- Small Box/Circle/Polygon colliders on furniture/NPCs: converted to AABBs.
- Trigger colliders: generally skipped unless the object is a physical static
  prop/chest; interaction is handled by markers.
- Furniture that needs a footprint rather than full sprite bounds gets an
  explicit builder rule (forge tables, object-portal tables, oxygen bottles).
- Doors/portals are trigger volumes; their interaction is handled by markers,
  not by treating the trigger as a wall.

The staircase at `(-10.405, -9.080)` is intentionally an open dark-void gap. Do
not restore the old 3x3 repair block. The thin floor-edge solid at the boundary is
correct.

## 12. Runtime asset refresh

The executable does not read `analysis/assets/` directly; it reads the asset root
resolved by `AssetEnvironment` (normally `build/assets/`). After any scene/asset
change:

```bash
./scripts/prepare-assets.sh
cmake --build build -j"$(nproc)"   # only needed if source changed
```

Common stale-asset symptoms:

- coordinate suite passes but the game looks unchanged;
- log shows an old instance count;
- a sprite is missing in the executable but present in
  `analysis/assets/scenes/...`;
- `build/assets/scenes/hero_room/scene.json` has an older timestamp than
  `analysis/assets/scenes/hero_room.json`.

Always compare timestamps or checksums when debugging.

## 13. Adding a new recovered room

1. Find the reference room prefab/GameObject name.
2. Add it to `build_pattern_rooms.py` or `build_visual_scene.py` input list.
3. Recover sprite metadata, floor/wall cells, doors, collision.
4. Generate `analysis/assets/scenes/<room>.json` + generated pixels.
5. Add the room to `RoomManager`/`DungeonPlan` metadata or biome pools.
6. Copy it to runtime assets via `prepare-assets.sh`.
7. Add a geometry area test and a smoke/activation test.
8. Validate that all doors have matching entrances and all floor cells are
   reachable.

## 14. Adding a new item/chest recipe

- Item prefabs are recovered by `build_item_assets.py`.
- Chest behavior is data-driven (`ChestDefinition`): `appointed_weapon`,
  `weapon_drop_level`, `boss`, `big`, `weapon_count`, `object_list`, `open_time`.
- The Living Room uses `hub_weapon_chest`: chest00 visuals/collider but random
  `weapon_drop_level = -2`. `chest00` itself is an appointed `weapon_021` chest
  and should not be used for a random starter chest.
- Workshop recipes come from `analysis/assets/workshop/weapon_materials.json`.
  Blueprint-gated IDs are the 17 recovered `blueprint_weapon_*` prefabs.

## 15. What is still reference-blocked

- No 11x11 PatternRoom in the recovered set.
- No recovered `PlantInfo` or `MechInfos` Resource tables for exact UI text.
- No dedicated Rogue sitting animation clip.
- Exact blueprint acquisition loop is not present in the recovered runtime data;
  the station gate and persistence are implemented, but the original source of
  each blueprint is not recoverable from the current artifacts.
- Exact reference save format is unknown; LiteKnight uses its own JSON schema.

When blocked, write the blocker in `docs/handbook/08-CURRENT_STATE_AND_ROADMAP.md`
or the relevant release note rather than inventing data.
