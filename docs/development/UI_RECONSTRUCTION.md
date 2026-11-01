# UI Reconstruction and Reference Evidence (0.5.2)

This document records what was searched, what was recovered, and what remains a
native desktop adaptation. It is the evidence trail for the 0.5.2 UI pass.

Labels:

- `CONFIRMED` — directly read from serialized reference data or a recovered
  sprite/prefab.
- `HIGH CONFIDENCE` — recovered assets plus a small, documented native
  composition decision.
- `HYPOTHESIS` — plausible native interpretation that is not proven to match
  the reference runtime.
- `UNKNOWN` — searched but not recovered.

## Evidence sources

- `level1` serialized scene: `title_scene`, `button_bar`, `btn_new_game`,
  `btn_multiplayer`, `btn_continue`, `button_setting`, `window_setting`.
- `level2` serialized scene: gameplay `Canvas`, `state_bar/bg`, `hp_bar`,
  `armor_bar`, `energy_bar`, `coin`, `btn_pause`, `map_info`, `item_info`,
  `window_pause`.
- `sharedassets2` / `SpriteAtlasTexture-ui`: HUD plate, bar fill, coin, pause,
  button frames, map icons, input icons, pickup frames.
- `sharedassets1` / `SpriteAtlasTexture-title`: main-menu backdrop and red swirl.
- `AssetBundles/common`: the same UI atlas used by prefabs and `UIMap` fields.
- `UIMap` IL2CPP fields and `SetMiniIcon` / `UpdateMap`:
  `boss_logo`, `special_logo`, `chest_logo`, `home_logo`, `cage_logo`,
  `color_0`, `color_1`, `color_2`, `map_manager`, `room_relation`.
- Weapon localization table and `WeaponInfo` TextAsset.
- User-provided WebP screenshots: `main menu.webp`, `HUD.webp`,
  `living room.webp`, `pause menu.webp` (inventory in
  `analysis/assets/webp_inventory.json`).

Searches covered Unity prefabs, `SpriteRenderer` components, `Image`
components, `RectTransform` anchors/pivots, `Text` components,
`SpriteAtlas` textures, `Animator` references, scene objects, `Resources`,
asset bundles, decompiled UI classes and localization strings. The remaining
items below are not "not searched"; they are either native composition choices
or genuinely missing from the recovered build.

## Main menu

### Backdrop and Knight artwork

```text
CURRENT APPROXIMATION: none (previously a native tunnel/gradient)
REFERENCE SEARCHES: level1 title_scene, SpriteAtlasTexture-title,
  SpriteRenderer/Image references, sharedassets1 textures
REFERENCE EVIDENCE FOUND: `biaoti 01` 1024x576 backdrop (tunnel, light beam and
  Knight silhouette); `biaoti 02` 500x847 swirl; `biaoti03`/`biaoti05png`
  Chinese title artwork
RECOVERABLE?: YES for backdrop/swirl; NO for a LIGHT KNIGHT English title sprite
IMPLEMENTATION: menu_backdrop + menu_swirl drawn as actual reference pixels;
  no runtime c00 sprite
CONFIDENCE: CONFIRMED reference art; LIGHT KNIGHT text is native
```

The WebP `main menu.webp` is a full-screen RGB screenshot. It has no alpha and
is not cropped into runtime art. The actual `biaoti 01` backdrop already
contains the Knight silhouette, so the previous "pick a c00 sprite" approach is
gone.

### Buttons

```text
CURRENT APPROXIMATION: vertical stack positions and width
REFERENCE SEARCHES: level1 title_scene/button_bar/btn_new_game/btn_multiplayer/
  btn_continue RectTransforms and Image sprites
REFERENCE EVIDENCE FOUND: reference button frames ui_btn5 (yellow), ui_btn1
  (brown), ui_btn7 (blue), ui_btn4 (red); reference button height 125 logical
  units, texture 18x17 with 9-slice
RECOVERABLE?: YES for frames and proportions; NO for a desktop vertical stack
  because the reference is a horizontal mobile bar
IMPLEMENTATION: actual frames, 560x148 rows, identical 14 px gap, single helper
  shared by draw and hit-testing, TextCentered labels
CONFIDENCE: HIGH CONFIDENCE for frames/proportions; HYPOTHESIS for vertical
  desktop composition
```

### Title

```text
CURRENT APPROXIMATION: LIGHT KNIGHT wordmark
REFERENCE SEARCHES: biaoti03, biaoti05png, title_en/title_cn, Text components
REFERENCE EVIDENCE FOUND: the recovered title art is Chinese "元气骑士" or the
  reference English "Soul Knight" brand, which must not be displayed
RECOVERABLE?: NO for an exact LIGHT KNIGHT artwork
IMPLEMENTATION: recovered pixel-bold font, LIGHT cream / KNIGHT gold, large
  left-aligned block
CONFIDENCE: HYPOTHESIS / native identity requirement
```

### Red decoration

```text
CURRENT APPROXIMATION: previously arbitrary red rectangles
REFERENCE SEARCHES: title_scene background/weijin; SpriteAtlasTexture-title
REFERENCE EVIDENCE FOUND: `biaoti 02` red brush swirl (500x847 source rect)
RECOVERABLE?: YES
IMPLEMENTATION: actual swirl sprite; native red line rectangles removed
CONFIDENCE: CONFIRMED
```

## Settings

```text
CURRENT APPROXIMATION: centered desktop rows
REFERENCE SEARCHES: level1 window_setting, window_setting buttons/title,
  ui_3 panel, ui_btn1 button frame
REFERENCE EVIDENCE FOUND: reference settings are a 235x720 left sidebar using
  ui_3 with icon rows (ui_btn1), a title using biaoti03/biaoti05png
RECOVERABLE?: PARTIAL — assets yes; exact mobile sidebar composition is not a
  desktop requirement
IMPLEMENTATION: actual ui_2/ui_3 panel and ribbon assets, actual ui_btn1 /
  ui_btn3 rows, disjoint label/value content rectangles, vertical centering
CONFIDENCE: HIGH CONFIDENCE for assets; HYPOTHESIS for desktop panel layout
```

## HUD

```text
CURRENT APPROXIMATION: formerly native HP/armor/energy panels and native icons
REFERENCE SEARCHES: level2 state_bar, hp_bar, armor_bar, energy_bar, coin,
  btn_pause, btn_skill, map_info; sharedassets2 ui atlas
REFERENCE EVIDENCE FOUND:
  - ui_15 full status plate with heart/shield/gem icons and three slots
  - ui_12 white bar fill (64x7)
  - ui_31 coin icon
  - ui_94 pause icon
  - ui_joy_0 circular button base
  - ui_joy_3 white lightning skill icon
  - ui_joy_7 blue-diamond recharge marker
  - ui_btn1 brown 9-slice frame
RECOVERABLE?: YES
IMPLEMENTATION: ui_15 plate derived from the editable HP/armor/energy rects;
  ui_12 tinted fractions; actual coin/pause/ability assets; compact minimap
CONFIDENCE: CONFIRMED assets and reference hierarchy; HYPOTHESIS for the
  desktop HUD anchor positions
```

## Minimap

```text
CURRENT APPROXIMATION: formerly native letters and a large panel
REFERENCE SEARCHES: level2 map_info root/a/room/target/Text, UIMap fields,
  multi_room map_info, UIMap.UpdateMap/SetMiniIcon
REFERENCE EVIDENCE FOUND:
  - map_info 200x200; 5x5 room grid; cell 24x24; 35 px pitch
  - room cells and a* connectors are Image quads with no sprite, tinted by
    UIMap.color_0/1/2
  - target Image uses ui_4 (cyan sparkle/player marker)
  - special icons: boss ui_14, special ui_114, chest ui_113, home ui_125,
    cage ui_193
  - no fullscreen map in the recovered HUD hierarchy
RECOVERABLE?: YES for construction, colors, target and special icons; UNKNOWN
  for the exact mobile Canvas anchor on a desktop monitor
IMPLEMENTATION: compact 240x196 HUD panel, plain tinted cells/connectors,
  actual home/boss/special/chest/cage icons when a room type is known, ui_4 for
  the current room, white D for the developer room
CONFIDENCE: CONFIRMED construction and assets; HYPOTHESIS for the native
  room-graph-to-grid mapping
```

The integrated graph currently contains the Living Room and three battle
rooms, so only the `home` special icon is active. Data fields for boss/special/
chest/cage are implemented and use the recovered sprites when a future room
type is supplied. The `D` devroom cell is intentionally native and uses a normal
cell size.

## Pause and pickup card

```text
CURRENT APPROXIMATION: centered desktop pause panel; pickup card
REFERENCE SEARCHES: level2 window_pause, title, btn_bar1, btn_continue,
  btn_home, btn_setting, img_hero; item_info/bg/value/long_text
REFERENCE EVIDENCE FOUND:
  - pause panel ui_2, ribbon ui_13, button frame ui_38, icons ui_98/ui_92/ui_93,
    portrait frame ui_22
  - item_info card frame ui_191; item bg ui_23; stat icons ui_33/34/35/37
RECOVERABLE?: YES for assets
IMPLEMENTATION: actual panel/ribbon/frames/icons; pickup card uses ui_191,
  weapon icon, actual name, damage and energy cost
CONFIDENCE: CONFIRMED assets; HYPOTHESIS for desktop panel sizes
```

## Text layout

The 0.5.2 pass found and fixed a global measurement bug: `UIRenderer::TextWidth`
and `LineHeight` returned world units while every caller passed logical-pixel
rectangles. This made centered labels start at the box center (not centered),
right-aligned values start at the right edge, and vertical centering use the
wrong baseline. Measurements are now converted to logical pixels, and
`TextFitted` wraps/shrinks in the same coordinate space. All button labels and
settings values use the same centered/aligned path and pixel snap.

## UI Design Mode

```text
CURRENT APPROXIMATION: native editor overlay
REFERENCE SEARCHES: no reference desktop UI editor exists
REFERENCE EVIDENCE FOUND: n/a
RECOVERABLE?: NO
IMPLEMENTATION: outlines and edits the actual HUD element rectangles; layout is
  saved in logical 1920x1080 coordinates; version 2 resets stale 0.5.1 anchors
  so the recovered HUD is not left at native old positions
CONFIDENCE: CONFIRMED native feature; HYPOTHESIS for a desktop editor UX
```

## Remaining UNKNOWN / HYPOTHESIS

- Exact reference desktop anchors for a 1920x1080 port (the reference is mobile).
- Exact LIGHT KNIGHT title artwork (the recovered artwork is Chinese/Soul
  Knight branding).
- Full room-type metadata for all special minimap icons; only `home` is used by
  the three integrated battle rooms today.
- Full per-weapon attack cadence for weapons outside the four integrated
  definitions.
- Exact `RGChest` loot tables and save-v3 per-room item state (unchanged from
  0.5.1).
