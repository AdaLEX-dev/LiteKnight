# Native Asset Pipeline

```text
local reference asset
    -> UnityPy extraction
    -> normalized metadata
    -> deterministic conversion
    -> validation
    -> generated native asset tree
    -> SceneLoader / runtime
```

## Entry points

| Stage | Command |
|---|---|
| Full local pipeline | `tools/asset/pipeline.py all` |
| Scene/metadata generation | `tools/asset/pipeline.py scene` |
| Build inventories | `tools/asset/pipeline.py inventory` |
| Metadata audit | `tools/asset/pipeline.py audit` |
| Local generated-pixel audit | `tools/asset/pipeline.py check-local` |
| Legacy scene builder | `tools/asset/build_visual_scene.py` |

`tools/asset/translators/README.md` lists the extracted categories and current
implementation status.

## Source vs generated

Source-controlled:

- `analysis/assets/scenes/r1_1_visual.json` and animation metadata
- `analysis/assets/texture_inventory.json`
- `analysis/assets/asset_inventory.json`
- scene/manifests/provenance and hashes
- native code, tests, tools, docs

Generated locally (gitignored):

- `assets/generated/visual/r1_1_visual/`
- `assets/generated/texture-conversion/`
- `analysis/apktool_out/`, `analysis/jadx_out/`, `analysis/il2cpp_dump/`

## Validation

`audit_assets.py` checks:

- unique IDs and source path IDs
- sprite rects inside texture bounds
- valid pivots / pixels-per-unit
- animation metadata existence
- character, weapon, projectile, enemy sprite references
- inventory count consistency
- redistribution status bookkeeping

Structural failures return non-zero. `--require-generated` additionally requires
local PNGs and is appropriate before a local release/smoke run.

## Adding a new category

1. Add extraction in `build_visual_scene.py` (or a new translator).
2. Add metadata and source IDs/hashes.
3. Add runtime loading/rendering.
4. Add tests.
5. Add the category to `build_asset_inventory.py`.
6. Run `pipeline.py all` and `audit`.
