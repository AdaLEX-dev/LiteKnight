# Reference Texture / Sprite / Prefab Gallery

## Purpose

`liteknight-texture-gallery` is a separate, locked-down asset laboratory. It is
not the game. It has no player, movement, combat, NPCs, enemies, map generation,
save data, inventory, currency, network access, or gameplay HUD. It draws a
neutral checkerboard, exactly one prepared asset, and its provenance metadata.

Use it to catch broken PNG conversions, wrong UVs, pivots, sprite rectangles,
filtering, atlas coordinates, texture IDs, scales, sorting, malformed prefabs,
missing textures and accidental event assets.

## Build / run

```bash
./scripts/build-texture-gallery.sh
./scripts/run-texture-gallery.sh
./build/bin/liteknight-texture-gallery
./build/bin/liteknight-texture-gallery --validate-all
```

The gallery resolves assets from `build/assets`, with an executable-relative
fallback (`build/bin` → `build/assets`). It does not need the original APK or
UnityPy at runtime.

## Controls

```text
Left / A       previous entry
Right / D      next entry
Space          next entry
Home           first entry
End            last entry
Escape         quit
```

Metadata shows `N / TOTAL`, name, type, source scene/bundle/path, texture key,
source rectangle and size, pivot, PPU, prefab part count and validation status.

## Enumeration source

`tools/asset/build_gallery_manifest.py` scans every prepared native scene
(`build/assets/scenes/*/scene.json`), the item pack, and the UI manifest. It
deduplicates textures by `source_path_id` and sprites by `sprite_path_id`, then
adds prefab-derived visual groups for:

- every weapon, enemy, character and pet definition
- every chest, coin, material, energy pickup and portal
- every unique obstacle_list prefab variant
- doors (leaf/shadow/ornament roles)
- every UI sprite

Output: `analysis/assets/reference_gallery_manifest.json`.

Current counts: **97 textures, 1931 sprites, 2500 prefabs, 4528 entries**.
Duplicates are intentionally collapsed; nonvisual assets (audio clips, text,
localization, collision-only data) are excluded by design. No visual asset is
silently omitted; broken entries remain in the manifest with a `reason`.

## Validation rules

For every texture: PNG exists, decodes, dimensions > 0 and texture ID exists.
For every sprite: texture resolves, rect is inside the texture, pivot is finite
and in `[0,1]`, PPU > 0. For every prefab: every part resolves to a valid
sprite, transforms are finite, and at least one visual part exists.

`--validate-all` performs the same checks headlessly and prints:

```text
ASSET AUDIT
Total entries: ...
Textures / Sprites / Prefabs
Broken: ...
```

The current manifest validates with **0 broken entries**.

## Living Room verification

The manifest includes the recovered default brown floor/wall tile atlas, the
green recycling-bin prefab, Living Room furniture, workshop/garden/trader
props, map icons, doors and the staircase-repair wall tiles
(`hub_spr_2093`/`hub_spr_2098`). The removed staircase is not recreated; the
old 3×3 opening is closed with the same `wall_u`/`wall_m` construction used by
the surrounding Living Room wall line, with a matching solid.
