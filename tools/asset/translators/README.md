# Native Asset Translators

The translation pipeline is intentionally staged:

```text
reference bundle -> UnityPy extraction -> normalized JSON metadata
                 -> generated native texture/runtime data -> validation
```

## Implemented translator entry points

| Category | Entry point | Output | Status |
|---|---|---|---|
| Texture2D | `extract_unity.py convert-texture` | PNG/RGBA8 + conversion manifest | implemented for controlled assets and scene atlases |
| Sprite / SpriteAtlas | `build_visual_scene.py` | `sprites[]` rect/pivot/PPU/UV metadata | implemented |
| SpriteRenderer / Prefab | `build_visual_scene.py` | `instances[]`, door/character/weapon/enemy metadata | implemented subset |
| AnimationClip / Animator | `build_visual_scene.py` | `animations/*.json` + clip references | implemented subset |
| Collider2D / room topology | `build_visual_scene.py` | collision solids, doors, obstacles | implemented |
| Character | `build_visual_scene.py` (`characters[]`) | body/dead sprites + ide/run clips | implemented subset |
| Weapon / Projectile | `build_visual_scene.py` (`weapons[]`) | four Gun001/projectile definitions + real weapon sprites | implemented subset |
| Enemy | `build_visual_scene.py` (`enemy_definitions[]`) | e_slime01 body/dead/shadow + stats | implemented subset |
| UI | developer `DebugTextRenderer` | generated placeholder 5x7 atlas | PLACEHOLDER; recovered UI atlas not integrated |
| Effects / items / audio | none yet | — | not recovered |

## Rules

1. Never commit proprietary pixels or raw bundle trees.
2. Every generated asset must retain source bundle/path ID and hashes.
3. The inventory has a redistribution classification; unknown means do not
   redistribute.
4. Add a translator when a new category is needed; do not hand-edit generated
   scene files.
5. `tools/asset/pipeline.py all` is the full local pipeline; CI uses only
   metadata audits because reference bundles are not redistributed.
