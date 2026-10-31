# Unity Sprite Metadata Findings

**Date:** 2026-10-31
**Reference:** Soul Knight 2.6.8, Unity 2018.4.12f1, metadata v24.
**Scope:** the controlled `r1_1` visual composition and its sprite/atlas path.

---

## 1. Serialized Sprite fields (CONFIRMED)

The native `SpriteData` design is based on fields read directly from UnityPy
1.25.4 objects, cross-checked against `dump.cs` property definitions:

| Unity field | Observed meaning | Confidence |
|---|---|---|
| `Sprite.m_Rect` | original sprite rect in pixels (`width`, `height`, `x`, `y`) | CONFIRMED |
| `Sprite.m_Pivot` | normalized pivot, Unity bottom-left origin | CONFIRMED |
| `Sprite.m_PixelsToUnits` | pixels per world unit; selected assets use **16** | CONFIRMED |
| `Sprite.m_Border` | 4-vector border; zero on selected sprites | CONFIRMED |
| `Sprite.m_RD.texture` | `Texture2D` PPtr | CONFIRMED |
| `Sprite.m_RD.textureRect` | atlas pixel rect, Unity **bottom-left** origin | CONFIRMED |
| `Sprite.m_RD.textureRectOffset` | offset of tight rect from the original sprite rect | HIGH CONFIDENCE |
| `Sprite.m_RD.settingsRaw` | SpriteRenderData settings; 67 on selected sprites | CONFIRMED value, semantics PARTIAL |
| `SpriteRenderer.m_Color` | per-instance tint, including alpha | CONFIRMED |
| `SpriteRenderer.m_FlipX/Y` | per-instance flip flags | CONFIRMED |
| `SpriteRenderer.m_SortingLayer` | sorting-layer index | CONFIRMED |
| `SpriteRenderer.m_SortingOrder` | sorting-order integer | CONFIRMED |
| `SpriteRenderer.m_DrawMode` | 0 (Simple) on all 109 room sprites | CONFIRMED |

`TagManager.m_SortingLayers` maps the index values:

```text
0 Default
1 BackGround
2 Floor
3 Shadow
4 Wall
5 Character
6 WallFont
7 Effect
8 UI
```

This matches the native `SortLayer` order. The numeric mapping is now evidence-backed,
not a guess.

## 2. Atlas/texture rect origin (CONFIRMED)

The selected atlas is `SpriteAtlasTexture-floor-256x256-fmt4`, Texture2D path ID
`5467841708608453938` in bundle `res/floor`, 256x256 RGBA32.

For `door01`:

```text
m_RD.textureRect = x=72, y=208, w=14, h=17
```

Cropping the atlas at top-left `(72,208)` did **not** equal `sprite.image`.
Cropping at bottom-left `(72, 256-208-17)` **did** equal `sprite.image` exactly.
Therefore Unity `textureRect.y` is bottom-origin. The native converter emits:

```text
native_rect.y = texture_height - unity_rect.y - unity_rect.height
```

and native UVs use a top-left normalized rect.

## 3. Pivot and cropped/tight sprites (HIGH CONFIDENCE)

For a tight crop, the native pivot is adjusted from the original rect and
`textureRectOffset`:

```text
local_left   = -source_pivot.x * source_rect.width  + texture_rect_offset.x
local_bottom = -source_pivot.y * source_rect.height + texture_rect_offset.y
pivot.x = -local_left   / tight_rect.width
pivot.y = -local_bottom / tight_rect.height
```

Example `c01_5` (player body):

```text
source_rect       = 32 x 32
source_pivot      = (0.6, 0.0)
texture_rect      = 25.8784 x 29.9219
texture_rect_offset = (6.1216, 0.0513)
adjusted pivot    = (0.50538, ~0.0)
```

The resulting local bounds `x[-0.8175, 0.8]`, `y[0, 1.873]` match the serialized
`m_VertexData` bounds. The native renderer uses the rectangular tight crop; the
transparent pixels within that crop preserve the silhouette for this visual
milestone. Arbitrary tight polygon meshes / `m_VertexData` index buffers are not
yet exported.

## 4. Selected sprite/texture set

### Room

- `r1_1`: 109 SpriteRenderers from `room/r1_1`.
- All 109 use the same Texture2D:
  `SpriteAtlasTexture-floor-256x256-fmt4` (256x256, RGBA32).
- Serialized sprite content:
  - 49 × `skin` (16x16, pivot 0.5/0, texture rect 126,174),
  - 20 × `door02` (16x24),
  - 20 × `shadow0` (16x24, pivot 0.5/0.3),
  - 20 × `door01` (14x17).
- Room world transforms are serialized; the reconstructed layout is a 7x7 floor
  block centered at (20,20) with four five-unit door clusters at the sides.

### Sprite object path IDs (CONFIRMED)

| Sprite | Object path ID | Texture |
|---|---|---|
| `skin` | `3625433789698647512` | `floor_atlas` |
| `door02` | `-6196740020252790978` | `floor_atlas` |
| `shadow0` | `6418692892874173330` | `floor_atlas` |
| `door01` | `-4876205757651736817` | `floor_atlas` |
| `c01_5` | `6827566613129973144` | `character_atlas` |
| `shadow3` | `-8753147750278292345` | `object_common_atlas` |

### Player

- `common/c00` hierarchy.
- `c00/img/body` uses Sprite `c01_5` from
  `SpriteAtlasTexture-character01 (Group 0)-2048x1024-fmt4`
  (Texture2D path ID `6092784074628815488`, RGBA32).
- `c00/shadow` uses `shadow3` from
  `SpriteAtlasTexture-object_common (Group 1)-32x32-fmt4`
  (Texture2D path ID `3188784419113904678`, RGBA32).
- Player visual is placed at `MapManager.centerPoint` evidence coordinate
  `(20,20)`, with the shadow at `(20,19.95)`.

### Texture import settings (CONFIRMED)

All three selected textures report:

```text
m_FilterMode = 0  // Point
m_WrapU = 1, m_WrapV = 1  // Clamp
```

The native sampler uses nearest filtering and clamp-to-edge.

## 5. Native data model

```text
TextureData  (pixels only)
    width, height, RGBA8

SpriteData   (interpretation only)
    texture reference/id
    top-left pixel rect
    adjusted pivot
    source pivot
    texture rect offset
    pixels per unit
    border
    filter/wrap

SpriteInstance
    sprite reference
    world position, rotation, scale
    color, flips, sorting layer/order
    source hierarchy path
```

No UnityPy dependency enters the runtime. `SceneLoader` consumes native JSON and
PNG bytes only.

## 6. Classification

| Claim | Confidence | Evidence |
|---|---|---|
| Sprite rect/pivot/PPU/border fields are serialized | CONFIRMED | UnityPy + `dump.cs` |
| PPU is 16 for selected sprites | CONFIRMED | serialized `m_PixelsToUnits` |
| `textureRect.y` is bottom-origin | CONFIRMED | exact atlas crop vs `sprite.image` |
| Tight-crop adjusted pivot formula | HIGH CONFIDENCE | bounds match `m_VertexData` |
| Room sprite atlas is shared | CONFIRMED | all 109 references same Texture2D |
| c00 body is `c01_5` | CONFIRMED technical | prefab SpriteRenderer |
| Player spawn is (20,20) | HIGH CONFIDENCE | `MapManager.centerPoint` evidence and room center |
| Exact camera orthographic size | UNKNOWN / PROJECT DECISION | no recovered reference camera value; 10.5 chosen from room bounds |
| Full tight polygon mesh support | NOT IMPLEMENTED | rectangular crop only |
| Full SpriteAtlas/pivot/PPU/atlas packing system | PARTIAL | one shared atlas relationship implemented |

## 7. Idle-frame sprite set

The selected `char_1_skin_0_ide` clip references four sprites from the same
character atlas:

| Sprite | Path ID | Texture rect (native top-left) | Adjusted pivot |
|---|---:|---|---:|
| `c01_4` | `-5734303052896741615` | `(1429.076, 594.027, 24.924, 28.922)` | `(0.486436, 0.0)` |
| `c01_5` | `6827566613129973144` | `(1572.122, 868.027, 25.878, 29.922)` | `(0.505379, 0.0)` |
| `c01_6` | `8630870737493093490` | `(1365.051, 819.027, 26.949, 29.922)` | `(0.525023, 0.0)` |
| `c01_7` | `9175778662407141084` | `(1562.036, 783.027, 25.964, 28.922)` | `(0.507004, 0.0)` |

All use PPU 16 and point sampling. Their slightly different widths/pivots are
recomputed on every frame, keeping the feet anchored. The animation sequencing
evidence is in `analysis/assets/animation.md`.

## 8. Remaining sprite work

- Export/consume exact `m_VertexData` and triangle/index buffers for tight meshes.
- SpriteAtlas object semantics and runtime atlas packing.
- Nine-slice/border rendering; border fields are preserved but not applied.
- Animator/AnimationClip frame selection.
- Full sprite metadata for all characters/enemies/UI.
