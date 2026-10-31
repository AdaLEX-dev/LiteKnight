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

## Font

`tools/asset/build_ui_font.py` extracts the recovered dynamic `pixel_bold` TTF,
rasterizes ASCII 32..126 at 28 px into a binary-alpha atlas, and records
per-glyph advance/bearing/UV metrics. `BitmapFont`/`TextRenderer` use those
metrics for measured, left/center/right aligned, multiline text with optional
shadow/outline. The generated atlas and TTF are local-only; the fallback
developer font keeps the executable usable without local assets.

The reference body font is `pixel_bold`; this is recovered, not a hand-made
replacement. UI glyphs are point-filtered and drawn at integer logical
coordinates.

## Screens

- HUD: top-left HP/armor/energy panel using recovered heart/shield/crystal
  icons; upper-right recovered pause/coin strip; bottom-left weapon slot strip;
  larger recovered `ui_skill01` Knight power button with `Q` label and cooldown
  mask; compact bottom-centre pickup card with damage/energy/cycle values.
- Pause: dark navy modal, red ribbon, character portrait, recovered power slots,
  Home / Resume / Settings.
- Settings: Aim Mode (Manual / Auto Aim), fullscreen, debug overlay, reset save,
  back. Only working settings are exposed. No social tab.
- Main menu: recovered `title_en0` logo, NATIVE LINUX DEVROOM subtitle, New Game,
  Continue, Settings, Quit. No social tab.
- Character station: 15 recovered body sprites and serialized HP/armor/energy
  stats; arrows browse, Enter confirms.
- Weapon station: four recovered weapons with real icons, damage, cost, cycle
  and projectile ID.

## Rendering notes

- UI is submitted through the existing SpriteBatch/Vulkan path; no web toolkit.
- Flat colors are converted from authored sRGB to linear before vertex upload,
  matching the sRGB swapchain.
- Pixel panels use hard-edged rectangles and borders; no gradients or rounded
  vector controls.
- HUD/status icons are recovered where mapped (`ui_33`, `ui_34`, `ui_67`,
  `ui_94`, `ui_5`, `ui_skill01`). Panel/ribbon frames and the main-menu
  background remain native approximations; exact frame mappings are UNKNOWN.
  Character/weapon icons are recovered sprites.
- All UI text uses the recovered 28 px `pixel_bold` atlas. Values below 28 are
  clamped to the native atlas size by `UIRenderer::FontScale`, so text is never
  downsampled; labels are measured/centered and kept inside their panels.
