# Unity Animation Findings

**Date:** 2026-10-31
**Reference:** Soul Knight 2.6.8, Unity 2018.4.12f1, metadata v24.
**Selected paths:** `common/c00` → Animator → `char_0_skin_0` → Base Layer
`ide`/`run` → `char_1_skin_0_ide` / `char_1_skin_0_run` → `c00/img/body`.
**Scope:** the two real locomotion clips. This is not a complete Unity Animator
reconstruction.

---

## 1. Selected animator graph

| Object | Path ID / value | Confidence |
|---|---:|---|
| `c00` GameObject | `-2381127569301570839` | CONFIRMED |
| `Animator` component | `-2747643251060265406` | CONFIRMED |
| `RuntimeAnimatorController` | `7636633949607769228` | CONFIRMED |
| Controller name | `char_0_skin_0` | CONFIRMED |
| State machine | `m_StateMachineArray[0]`, Base Layer | CONFIRMED |
| Default state index | `0` | CONFIRMED |
| Animator state | `ide`, path ID `580769968` | CONFIRMED |
| State full path hash | `580769968` (`Base Layer.ide`) | CONFIRMED |
| State speed / loop | `1.0` / `true` | CONFIRMED |
| Idle `AnimationClip` | `char_1_skin_0_ide`, path ID `-6496688600943111911` | CONFIRMED |
| Run state | `run`, path ID `4142231225` (`Base Layer.run`) | CONFIRMED |
| Run `AnimationClip` | `char_1_skin_0_run`, path ID `3681644519574295089` | CONFIRMED |
| Clip start / stop | `0.0` / `0.5` seconds for both | CONFIRMED |
| Clip sample rate | `8.0` for both | CONFIRMED |
| Clip loop | `true` (`m_LoopTime`) for both | CONFIRMED |

The Animator serializes `m_UpdateMode = 0` (**Normal**) and
`m_ApplyRootMotion = false`. The prefab Animator component has
`m_Enabled = false`; the runtime path that enables it in the original game has not
been recovered. These facts are preserved in the native source metadata.

The controller also contains `dead` and a HitLayer. Only `ide` and `run` are
implemented natively.

## 2. Binding and sprite references

The selected clip has one bind to `c00/img/body`:

```text
GenericBinding
  path        = 3966442053  // CRC32("img/body")
  attribute   = 0
  typeID      = 212         // SpriteRenderer
  customType  = 23
  isPPtrCurve = 1
  property    = m_Sprite
```

Confidence: **CONFIRMED** for the serialized binding fields.

`m_PPtrCurves` on this clip is **empty**:

```text
serialized_pptr_curve_count = 0
```

However, the clip's `m_ClipBindingConstant.pptrCurveMapping` contains exactly
the four body sprites:

| Mapping index | Sprite | Sprite path ID | Source rect |
|---:|---|---:|---|
| 0 | `c01_4` | `-5734303052896741615` | 32×32 at (0,64) |
| 1 | `c01_5` | `6827566613129973144` | 32×32 at (32,64) |
| 2 | `c01_6` | `8630870737493093490` | 32×32 at (64,64) |
| 3 | `c01_7` | `9175778662407141084` | 32×32 at (96,64) |

All four reference Texture2D `SpriteAtlasTexture-character01 (Group 0)` path ID
`6092784074628815488`, RGBA32, 2048×1024. Confidence: **CONFIRMED**.

The clip's `StreamedClip` has four curves. The curve at index 3 (the
SpriteRenderer PPtr curve position) contains:

| Frame time | Raw value | Mapping result |
|---:|---:|---|
| `-inf` sentinel | `0.0` | `c01_4` |
| `0.0` (no explicit key; initial value) | `0.0` | `c01_4` |
| `0.125` | `1.0` | `c01_5` |
| `0.250` | `2.0` | `c01_6` |
| `0.375` | `3.0` | `c01_7` |
| `0.500` | loop boundary | wraps to `c01_4` |

Confidence: **HIGH CONFIDENCE**. The evidence is the exact four-entry
`pptrCurveMapping`, the pptr binding, and streamed values that are exactly the
mapping indices. UnityPy's high-level `m_PPtrCurves` field does not expose the
keys for this build/fixture, so the converter reads the binding mapping and
streamed curve directly instead of pretending the empty list contains data.

The `pptrCurveMapping` name/order and the four distinct images were also checked
against the atlas pixels. The frame sequence visibly differs in the Knight's
scarf and eye highlight.

### Run mapping

The run clip uses the same `SpriteRenderer.m_Sprite` binding and a different
four-entry pptr mapping:

| Mapping index | Sprite | Sprite path ID | Source rect |
|---:|---|---:|---|
| 0 | `c01_0` | `-4648659440509418814` | `(559.076, 623.076, 31.924, 28.897)` |
| 1 | `c01_1` | `-4226452341420905142` | `(32.000, 587.076, 32.000, 29.897)` |
| 2 | `c01_2` | `645446436195162188` | `(170.000, 497.051, 32.000, 29.922)` |
| 3 | `c01_3` | `-3975174193321666585` | `(953.051, 298.076, 31.949, 28.897)` |

These are Unity `textureRect` values in bottom-origin coordinates. The native
top-left rects and adjusted pivots are recorded in
`analysis/assets/scenes/animations/c00_run.json`.

The run binding's PPtr curve is streamed at index `6` and supplies values
`0,1,2,3` at times `0, 0.125, 0.25, 0.375`, then wraps at `0.5`. This is
**HIGH CONFIDENCE** for the same reasons as idle: the binding is the same
`img/body` SpriteRenderer property, `pptrCurveMapping` contains exactly the four
run sprites, and the streamed curve values are exactly mapping indices.

## 3. Native conversion

`tools/asset/build_visual_scene.py` derives:

```text
char_1_skin_0_ide
  duration = 0.5
  sample_rate = 8.0
  loop = true
  track target = c00/img/body
  property = m_Sprite
  keys:
    0.000 -> c01_4
    0.125 -> c01_5
    0.250 -> c01_6
    0.375 -> c01_7

char_1_skin_0_run
  duration = 0.5
  sample_rate = 8.0
  loop = true
  track target = c00/img/body
  property = m_Sprite
  keys:
    0.000 -> c01_0
    0.125 -> c01_1
    0.250 -> c01_2
    0.375 -> c01_3
```

All four sprites reuse the already converted character-atlas PNG; no duplicate
texture bytes are written. The committed native metadata is
`analysis/assets/scenes/animations/c00_idle.json`.

Deterministic scene generation:

- Scene metadata SHA-256 (current topology pass):
  `e7721abf2d90121cb0165c6f0cd1363b8f723c7574d3b09ca9a5780033b1e1d0`
- Idle animation metadata SHA-256:
  `23f34e0a6e3b5eb7d11f2e7dcbb11d89c25a9f2b8a17360ff70c280e846f7d44`
- Run animation metadata SHA-256:
  `5a464af90c3e61c4b5e3ac6340583234bf6cae114d0a0f281980f2c80e5a3693`
- Converted character atlas GPU bytes are unchanged:
  RGBA8 `7c161a0d99dc595de33fe7df43763efc37ceaed904774a896e49c7f0e3e356cc`,
  PNG `4f5be052f7cb07e12913bbbe0945178a4f789c6aa492dbeda78becc29fec32e6`.

## 4. Sampling semantics (implemented native decision)

Unity stores the sprite as a discrete object-reference curve. The native
`SpriteTrack::Evaluate` therefore uses step semantics:

- `t == key.time` selects that key.
- `t` between keys selects the preceding key.
- `t` before the first key selects the first key.
- Duplicate-time keys select the last serialized key.
- At `t == duration` a loop wraps to time `0` and increments the loop count.
- A non-looping clip clamps to the final key at `duration`.

Both selected clips have explicit body keys at `0`, `0.125`, `0.25`, and
`0.375`; `0.5` is the loop boundary. The runtime uses integer `fixedDeltaTime`
of `0.02 s` for deterministic simulation, while the native sampling interpolates
nothing. The serialized Animator update mode is Normal, so evaluating animation
on a render-frame clock may better match Unity; this milestone intentionally
uses the fixed clock for reproducibility and records that distinction as
**NATIVE DESIGN**, not Unity parity. `LocomotionAnimator` selects `ide` for zero
movement and `run` for nonzero movement, resetting the selected clip to time 0 on
a state change.

## 5. Classification summary

| Claim | Confidence |
|---|---|
| Animator/controller/state/clip IDs and names above | CONFIRMED |
| Default state is `ide`; clip is `char_1_skin_0_ide` | CONFIRMED |
| Clip duration 0.5 s, 8 Hz, loop true | CONFIRMED |
| Binding is `img/body` `SpriteRenderer.m_Sprite` | CONFIRMED |
| `m_PPtrCurves` list is empty on this clip | CONFIRMED |
| `pptrCurveMapping` is the four c01_4..c01_7 sprites | CONFIRMED |
| Streamed curve 3 supplies idle mapping indices 0..3 at 0/.125/.25/.375 | HIGH CONFIDENCE |
| Run state/clip and four-sprite mapping are serialized | CONFIRMED |
| Streamed curve 6 supplies run mapping indices 0..3 at 0/.125/.25/.375 | HIGH CONFIDENCE |
| Idle/run selection is driven by movement presence | NATIVE DESIGN |
| Full Animator transitions/layers/blend semantics | UNKNOWN / NOT IMPLEMENTED |
| Runtime enables the prefab Animator before playback | UNKNOWN |
| Exact Unity Normal-vs-fixed sampling parity | UNKNOWN |

## 6. Limitations

- Only `ide` and `run` are reconstructed; `dead`, HitLayer, parameters and
  transitions are not implemented.
- The native clip model has one SpriteRenderer property track. It does not model
  arbitrary generic curves, state parameters, blend trees, IK, events, or root
  motion.
- Animator update-mode parity is not established.
- The original `m_PPtrCurves` absence is documented, not silently normalized into
  a claim of direct serialized PPtr keyframes.
