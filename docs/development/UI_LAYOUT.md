# HUD Layout and UI Design Mode

**Status:** implemented native layout editor; coordinates are logical 1920×1080.

## Coordinate model

All HUD rectangles are authored in the 1920×1080 logical design space. The
`LogicalViewport` maps that space to the framebuffer/window with an
aspect-preserving, integer-favouring scale and centered borders. Design Mode and
gameplay use the same `WindowPointToLogical` mapping, so mouse drags and HUD
presentation stay correct in windowed, bordered fullscreen, letterboxed and
pillarboxed modes.

## Layout file

`$XDG_CONFIG_HOME/liteknight/ui_layout.json`:

```json
{
  "version": 2,
  "coordinate_space": "logical_1920x1080",
  "elements": {
    "hp":     {"x": 90, "y": 26, "w": 276, "h": 36},
    "armor":  {"x": 90, "y": 72, "w": 276, "h": 36},
    "energy": {"x": 90, "y": 118, "w": 276, "h": 36},
    "coin":   {"x": 1652, "y": 20, "w": 118, "h": 44},
    "pause":  {"x": 1812, "y": 16, "w": 92, "h": 82},
    "minimap": {"x": 1664, "y": 112, "w": 240, "h": 196},
    "ability": {"x": 20, "y": 846, "w": 150, "h": 150},
    "weapon_slots": {"x": 190, "y": 986, "w": 220, "h": 80},
    "active_weapon": {"x": 190, "y": 940, "w": 220, "h": 30},
    "pickup_prompt": {"x": 700, "y": 886, "w": 520, "h": 148},
    "stage": {"x": 16, "y": 190, "w": 360, "h": 26}
  }
}
```

The three status rectangles are the actual editable bar slots inside the
recovered `ui_15` plate; the plate bounds are derived from their union. There is
no `map_panel` in the HUD layout because 0.5.2 uses the compact minimap only.

Version 2 reset: a version 1 layout file is rejected and the new defaults are
used. This is intentional because version 1 stored the old native anchors,
which would leave the recovered HUD plate and compact minimap visually
misaligned. A version 2 file preserves every user edit.

The file is human-readable, versioned and loaded at startup. Missing files use
the defaults above. Invalid/malformed files fall back to defaults with a log
warning. Only coordinates are edited; widths/heights remain the authored layout
contract.

## Design Mode

`Settings -> UI DESIGN MODE` enters the editor. The simulation is frozen. A
click selects the top-most HUD element under the logical mouse position; dragging
updates its logical x/y; mouse release saves. `S` saves, `R` restores defaults,
`ESC` saves and returns to Settings.

## Evidence boundary

`CONFIRMED`: recovered `ui_15`, `ui_12`, `ui_31`, `ui_94`, `ui_joy_0/3/7`,
`ui_btn1/3/4/5/7`, `pixel_bold`, weapon/character sprites and UIMap cell colors
and icons.

`HYPOTHESIS`: exact desktop HUD pixel anchors. The reference is a mobile
Canvas; the native layout places the recovered elements at logical 1920×1080
corners. Borders, the compact minimap size and the desktop vertical main-menu
stack are presentation policy, not recovered serialized values.
