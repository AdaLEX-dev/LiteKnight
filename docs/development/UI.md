# Native 1920×1080 Pixel UI

**Status:** functional for HUD, pause, settings, main menu, character station
and weapon station. Reference-derived pixels/font are `LOCAL_ONLY`.

## Coordinate contract

```text
logical design space: 1920×1080, 16:9, top-left origin for UI
framebuffer:           whatever Wayland/Vulkan provides
presentation:          uniform integer-favouring scale, letterbox/pillarbox
world camera:          same 1920×1080 aspect contract
```

`LogicalViewport` records framebuffer/window dimensions, scale, viewport offset
and aspect. `UIRenderer` maps integer logical coordinates to world coordinates.
The F10 developer overlay displays:

```text
LOGICAL: 1920x1080  FRAMEBUFFER: WxH
SCALE: XxY  ASPECT: 16:9
```

Mouse aim is converted from window points through the same viewport, so
pillarbox offsets do not shift the aim vector.

## Font and text layout

`tools/asset/build_ui_font.py` extracts the recovered dynamic `pixel_bold` TTF,
rasterizes ASCII 32..126 at 28 px into a binary-alpha atlas, and records
per-glyph advance/bearing/UV metrics. `BitmapFont`/`TextRenderer` use those
metrics for measured, left/center/right aligned, multiline text with optional
shadow/outline. The generated atlas and TTF are local-only; the fallback
developer font keeps the executable usable without local assets.

`UIRenderer` exposes one layout path in logical pixels: measure first, align
left/center/right, center vertically, optionally wrap/fit, then pixel-snap the
final position. `TextCentered` is used for every button, ribbon and prompt
label; settings rows use disjoint label/value rectangles. The 0.5.2 pass fixed
measurements that were previously returned in world units, which had caused
labels to drift right of center and values to spill out of boxes.

The reference body font is `pixel_bold`; this is recovered, not a hand-made
replacement. UI glyphs are point-filtered and drawn at integer logical
coordinates.

## Screens

- HUD: recovered `ui_15` status plate with `ui_12` bar fills; recovered
  `ui_31` coin and `ui_btn1`/`ui_94` pause button; lower-left recovered
  `ui_joy_0`/`ui_joy_3` lightning ability with `ui_joy_7` recharge marker;
  bottom-left weapon strip; compact bottom-centre `ui_191` pickup/chest card.
- Minimap: compact dark HUD panel with reference UIMap construction: plain
  tinted room cells/connectors, current/visited/unexplored colors, recovered
  home/boss/special/chest/cage icons and `ui_4` current marker; white-D
  developer cell. No fullscreen map overlay.
- UI Design Mode: mouse-draggable logical HUD layout, saved to
  `$XDG_CONFIG_HOME/liteknight/ui_layout.json` (see `UI_LAYOUT.md`).
- Pause: recovered `ui_2` panel, `ui_13` ribbon, `ui_22` portrait frame,
  recovered `ui_98`/`ui_92`/`ui_93` icons and `ui_38` frames; Home /
  Resume / Settings.
- Settings: Aim Mode (Manual / Auto Aim; Auto Aim is the fresh default),
  fullscreen, debug overlay, UI Design Mode, reset save, back. Only working
  settings are exposed. No social tab.
- Main menu: recovered `menu_backdrop` Knight/tunnel art, recovered
  `menu_swirl`, LIGHT KNIGHT pixel wordmark, blue opaque native D-ROOM
  developer button, and New Game / Continue / Settings / Quit rows using the
  recovered yellow/blue/brown/red button frames. The recovered title artwork
  is Chinese/Soul Knight branding, so the required LIGHT KNIGHT identity uses
  the recovered font. No social tab.
- Character station: 15 recovered body sprites and serialized HP/armor/energy
  stats; arrows browse, Enter confirms.
- Weapon station: 168 Tier A recovered projectile weapons with real icons,
  damage, cost, cycle and projectile ID, paginated across the 302-entry catalogue.

## Rendering notes

- UI is submitted through the existing SpriteBatch/Vulkan path; no web toolkit.
- Flat colors are converted from authored sRGB to linear before vertex upload,
  matching the sRGB swapchain.
- Pixel panels use hard-edged rectangles and borders; no gradients or rounded
  vector controls.
- Recovered UI/native evidence and remaining approximations are catalogued in
  `docs/development/UI_RECONSTRUCTION.md`. Actual sprites used by the HUD,
  pause, settings, main menu, pickup card and map include `ui_15`, `ui_12`,
  `ui_31`, `ui_94`, `ui_joy_0/3/7`, `ui_btn1/3/4/5/7`, `ui_2`, `ui_3`,
  `ui_13`, `ui_22`, `ui_38`, `ui_98/92/93`, `ui_191`, `ui_14`,
  `ui_113`, `ui_114`, `ui_125`, `ui_193`, `ui_4`, `menu_backdrop` and
  `menu_swirl`.
- All UI text uses the recovered 28 px `pixel_bold` atlas. Values below 28 are
  clamped to the native atlas size by `UIRenderer::FontScale`, so text is never
  downsampled. Measurement and alignment are logical-pixel based and pixel
  snapped.
