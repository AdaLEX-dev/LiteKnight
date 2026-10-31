# 2D Coordinate System Findings

**Date:** 2026-10-31
**Scope:** native `Camera2D`, room world coordinates, sprite pivots, and Vulkan
projection for the controlled `r1_1` visual composition.

---

## 1. Recovered values

| Property | Value | Classification | Evidence |
|---|---|---|---|
| Sprite pixels-per-unit | 16 | CONFIRMED | selected Sprite `m_PixelsToUnits` |
| Native world Y direction | **Up** | PROJECT DECISION matching Unity 2D | serialized Transform positions and sprite pivots |
| Sprite pivot origin | bottom-left normalized | CONFIRMED | `m_Pivot`; `(0.5,0)` bottom center |
| Room visual center | `(20,20)` | HIGH CONFIDENCE | `r1_1` serialized layout; `MapManager.centerPoint` evidence |
| Player spawn | `(20,20)` | HIGH CONFIDENCE | recovered `MapManager.centerPoint` |
| Room visual extent | ~`x[12,28]`, `y[12,29]` for doors/walls; floor `x[17,23]`, `y[17,23]` | CONFIRMED from transforms | `analysis/assets/scenes/r1_1_visual.json` |
| Camera orthographic size | **10.5 world units half-height** | PROJECT DECISION / UNKNOWN reference value | chosen to fit the recovered room composition |
| Logical viewport | 1920×1080, 16:9 | CONFIRMED target | project target |
| Camera center | `(20,20)` | HIGH CONFIDENCE | room center/spawn |

The exact reference orthographic camera size has not been recovered from a
serialized scene/camera object. It remains a configurable native parameter and
must not be presented as Unity parity.

## 2. World and screen conventions

- Native world: +X right, +Y up.
- Sprite pivots: normalized, `(0,0)` bottom-left of the sprite rect.
- Sprite rects: top-left pixel coordinates in the native texture data; the
  converter flips Unity's bottom-left texture rect exactly once.
- Camera: center position, `orthographic_size` = half-height in world units.
- Screen: top-left origin, +Y down (Vulkan/window convention).
- Vulkan projection maps world +Y (top) to NDC Y = -1 and world -Y (bottom) to
  NDC Y = +1. This is the explicit Y inversion in `Camera2D::ViewProjection`.
- A 16:9 logical viewport is aspect-fit into the compositor-sized swapchain;
  unused bars remain the clear color.

`Mat4` stores column-major data (`m[col * 4 + row]`). A bug in `Mat4::operator*`
(previous row-major multiplication) dropped camera translation; it was fixed and
covered by a `Camera2D::ViewProjection().TransformPoint(center) == (0,0)` test.

## 3. Camera transform

For camera center `C`, half-height `H`, aspect `A = width/height`:

```text
half_width = H * A
screen_x = width  / 2 + (world_x - C.x) / (2 * half_width)  * width
screen_y = height / 2 - (world_y - C.y) / (2 * H)           * height
```

The inverse is implemented in `Camera2D::ScreenToWorld`.

Tests cover:

- world center → screen center,
- screen/world round-trip,
- aspect ratio,
- camera movement,
- zoom changing half extents,
- the actual Vulkan projection center.

## 4. Room/player placement

The serialized `r1_1` hierarchy resolves to:

- 49 `skin` floor/wall tiles in a 7×7 block centered at `(20,20)`,
- four door clusters at the room edges,
- `c00` body at the player spawn `(20,20)`,
- `c00` shadow at `(20,19.95)`.

The native composition is derived from `analysis/assets/scenes/r1_1_visual.json`.
The camera is centered at the spawn with half-height 10.5, showing the full room
and player without using monitor resolution.

## 5. Remaining coordinate questions

- Exact reference camera orthographic size and camera follow behavior.
- Whether the reference room root applies additional offsets/scaling at runtime.
- SpriteRenderer flip/pivot behavior for all animations and tight meshes.
- World-to-screen integration for UI/canvas coordinates.
