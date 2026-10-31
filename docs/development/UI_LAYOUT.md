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
  "version": 1,
  "coordinate_space": "logical_1920x1080",
  "elements": {
    "hp":     {"x": 16, "y": 12, "w": 352, "h": 38},
    "armor":  {"x": 16, "y": 54, "w": 352, "h": 38},
    "energy": {"x": 16, "y": 96, "w": 352, "h": 38},
    "coin":   {"x": 1652, "y": 20, "w": 118, "h": 44},
    "pause":  {"x": 1812, "y": 16, "w": 92, "h": 82},
    "minimap": {"x": 1600, "y": 112, "w": 304, "h": 178},
    "ability": {"x": 190, "y": 920, "w": 128, "h": 128},
    "weapon_slots": {"x": 20, "y": 986, "w": 160, "h": 86},
    "active_weapon": {"x": 20, "y": 930, "w": 160, "h": 30},
    "pickup_prompt": {"x": 730, "y": 946, "w": 460, "h": 112},
    "stage": {"x": 16, "y": 142, "w": 352, "h": 26},
    "map_panel": {"x": 520, "y": 170, "w": 880, "h": 740}
  }
}
```

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

`CONFIRMED`: recovered `ui_33`, `ui_34`, `ui_67`, `ui_94`, `ui_5`, `ui_skill01`,
`ui_btn1/5/7`, `pixel_bold`, weapon/character sprites.

`HYPOTHESIS`: exact reference HUD pixel anchors; the default layout is a
native 1920×1080 arrangement that keeps recovered elements flush to the intended
corners. Borders/axis padding are presentation policy, not recovered serialized
values.
