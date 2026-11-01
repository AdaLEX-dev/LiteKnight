# 05 — UI and Rendering

## 1. Render pipeline

Files:

```text
src/engine/rendering/VulkanRenderer.*
src/engine/rendering/SpriteRenderer.*
src/engine/rendering/SpriteBatch.*
src/engine/rendering/Camera2D.*
src/engine/rendering/LogicalViewport.*
src/game/ui/UIRenderer.*
shaders/
```

Frame:

```text
application batch -> sprite_batch_ -> VulkanRenderer -> swapchain present
UI batch         -> ui_renderer_  -> same Vulkan renderer, later layers
```

Rendering is immediate-mode collection into a batch, then one upload/draw pass.
The renderer is generic: it draws `SpriteDraw` records with a texture, UV, size,
position, pivot, rotation, flips, color, sorting layer/order.

## 2. Logical viewport and letterboxing

The design resolution is 1920x1080. The actual window can be any size. The
viewport is aspect-fit with letterbox bars. `LogicalViewport` converts:

- window pixels -> logical UI coordinates;
- mouse pixels -> world coordinates via camera.

Startup logs a `Viewport contract` line with drawable size, viewport rect, scale,
and borders. If mouse clicks land in the wrong place, compare the logical
viewport and the UI rect helper before changing anything.

## 3. Camera and world coordinates

- Y-up world.
- Orthographic size default 10 world units; view width depends on aspect.
- Camera follows player with clamping to room bounds.
- `TeleportForReview` sets player/camera and clamp; used by review modes.
- UI is drawn in logical pixels, independent of world camera.
- World-to-screen helpers exist for pickup/interaction/stake/dialogue labels.

## 4. Depth sorting

- `SortLayer` enum maps recovered sorting layers to render order.
- `WorldDepthOrder(y, tie_break)` converts world Y to a sorting order.
- Ground props use foot Y. Props that should sit behind characters/floor can set
  `ground_depth: false` (for example the `lazy_sofa` beanbag).
- `sorting_layer` and `sorting_order` from the recovered SpriteRenderer are
  preserved where possible.
- The player body, weapon, muzzle, and world labels use explicit tie-breakers.

Common depth bugs:

- a prop inside a chair/table because foot Y is too low/high;
- a beanbag drawn in front of the Rogue;
- a weapon behind the body because depth used the hand Y instead of foot Y;
- a floor overlay drawn above characters.

## 5. UI screens

`UIScreen`:

```text
Gameplay, Pause, Settings, MainMenu, CharacterSelect,
WeaponSelect, SaveLoad, Trader, PetSelect, Workshop, MaterialChest
```

`GameUI` draws all screens. `Application` builds `GameUIData` from live systems.
`UIAction` is the command bus. Pointer hit-testing and drawing must use the same
rect helper. The main menu has one authoritative layout (`MainMenuButtonRect`,
`MainMenuSaveLoadRect`, `MainMenuDroomRect`).

Input rules:

- `Esc` closes the current screen/back/pause.
- `Enter` confirms.
- Arrow keys/WASD move selection where a list/card screen exists.
- Mouse clicks on buttons dispatch the same actions as keyboard.
- UI clicks play `ui_click` through `PlayUiSound`.

## 6. HUD

Files:

```text
src/game/ui/HudLayout.*
src/game/ui/GameUI.cpp
config/default.toml
$XDG_CONFIG_HOME/liteknight/ui_layout.json
```

- HP/armor/energy bars, coins/orbs, pause button, ability icon, weapon slots,
  minimap, room label, pickup/interaction labels, dialogue overlay.
- Logical layout is data-driven and can be edited in UI Design Mode.
- Layout persistence is separate from game save.
- `M` toggles the map; the minimap is compact and always visible where enabled.
- Room title/clear banner animation comes from recovered `UIMessageBar` timing.
- Pickup card and interaction prompt use world-projected positions.

The weapon slots show the active weapon sprites. Walking must not animate the
HUD weapon transforms.

## 7. Weapon presentation (hard rule)

Files:

```text
src/game/weapons/WeaponRenderer.hpp
src/game/weapons/WeaponRenderer.cpp
src/app/Application.cpp (draw call)
tests/unit/weapon_presentation_test.cpp
```

Authoritative API:

```cpp
WeaponPresentation WeaponRenderer::ComputePresentation(
    const WeaponDefinition& weapon,
    const Vec2& player_position,
    const Vec2& aim_direction,
    const Vec2& hand_offset,
    float visual_time,
    bool visual_attack_active,
    float effect_scale = 1.0f);
```

Rules:

- `position = player_position + hand_offset + weapon.visual_offset` plus only a
  tiny recoil offset when attacking.
- `rotation = aim_angle + weapon.visual_rotation` plus a tiny recoil rotation
  when attacking.
- Walking/locomotion never enters this function.
- Aim rotates around the fixed anchor; it does not translate.
- Recoil comes from recovered curves, scaled by `effect_scale` (0.22 in normal
  play, 1.0 in full effect mode).
- Recoil rotation curve values are degrees; convert to radians.
- Muzzle exists only while `visual_attack_active` is true and the muzzle curve
  says visible.
- No muzzle sprite => no muzzle effect.
- Muzzle position derives from the weapon presentation, not from a random or
  hardcoded screen-space point.
- Draw depth uses foot Y, not hand Y.
- `DrawAt` is for off-hand/Dual Wield; it uses the same presentation math.

If a future weapon family needs a different presentation, add a data field or a
clearly scoped branch inside `ComputePresentation`; do not add per-call-site
offsets in `Application`.

## 8. Muzzle effects

- Muzzle sprites are recovered per weapon (`muzzle_sprite`).
- `muzzle_enabled` curves control visibility.
- Muzzle scale is `weapon.visual_scale * effect_scale`.
- The default is tiny/short/localized.
- If a weapon has no recovered muzzle, leave it empty. Do not reuse the pistol
  muzzle for every gun.

## 9. Station UIs

### Trader UI

- Three cards, name, damage, energy, price/sold.
- Prices use recovered `item_value`.
- Selection is left/right or mouse.
- Buying deducts orbs, marks sold, persists.

### Workshop / Weapon Station

- List of recovered recipes on the left; detail on the right.
- Shows weapon sprite, name, damage, energy, materials with owned counts.
- Locked blueprint state is gold `BLUEPRINT`; the detail says
  `NEED TO COLLECT WEAPON BLUEPRINT FIRST`.
- `READY`/`MISSING` state is based on materials and blueprint.
- `Enter` crafts; `D` dismantles the active weapon; `Esc` closes.
- Do not grant a weapon if the craft fails.

### Material Chest

- Dedicated `MATERIALS` screen, not the forge.
- Each row: actual material icon, name, quantity.
- Empty storage shows `NO MATERIALS IN STORAGE`.
- Review mode seeds iron/wood/gem/gear so the icon layout is inspectable.
- The UI is intentionally a storage list, not a text-only inventory.

### Mech Platform

- Enlarged world-space panel (18.0 x 10.6 world units in the current pass).
- Larger fonts for name, cost, stats, blueprint/lock state, orb cost, selection.
- The rest of the HUD is not scaled.
- `Up/Down` selects, `Enter` builds/stores, `Esc` closes.
- Building deducts orbs and sets the mount.

### Pet / Character / Weapon / SaveLoad

- Pet select uses recovered pet sprites.
- Character select shows recovered bodies and names.
- Weapon select uses the weapon catalogue and sprite metadata.
- SaveLoad lists native slots; not reference-compatible.

## 10. Review modes and visual inspection

The executable supports named review areas via `--review-area`/`--area`.

Examples:

```bash
./build/soul-knight --hub --review-area table-characters
./build/soul-knight --hub --review-area rogue
./build/soul-knight --hub --review-area portal
./build/soul-knight --hub --review-area oxygen
./build/soul-knight --hub --review-area material-chest-ui
./build/soul-knight --hub --review-area weapon-station-locked
./build/soul-knight --hub --review-area mech-ui-live
./build/soul-knight --hub --review-area trader-ui
./build/soul-knight --hub --review-area trader-world
```

The review branch teleports the player south of the subject and opens the
matching UI for station areas. For a screenshot on the current machine:

```bash
./build/soul-knight --hub --review-area rogue --smoke-seconds 6 &
sleep 4.5
spectacle -b -n -f -o /tmp/rogue.png
```

Crop the application window if needed. A geometry test cannot prove visual
correctness; screenshots and pixel checks are required for visual-only issues.

## 11. Sprite atlas and UI assets

- UI atlases are loaded by `UIAssets` from generated UI textures.
- `UIRenderer` uses triangle/quad batches in logical coordinates.
- Scene sprites use `SceneSprite` with texture/sprite IDs.
- Recovered UI sprite IDs (`ui_5`, `ui_33`, `ui_52`, etc.) are evidence-backed
  selections; do not substitute a generic rectangle when the recovered icon is
  available.
- Nine-slice panels use `ui_btn*`, `ui_15`, etc. where implemented.

## 12. Common rendering/UI bugs

| Symptom | Check |
|---------|-------|
| Weapon moves while walking | Did any call site add locomotion offset? `WeaponRenderer::ComputePresentation` only. |
| Weapon points wrong way | `aim_direction`, rotation sign, `flip_y` for left aim. |
| Muzzle always visible | `visual_attack_active` must gate it; check muzzle curve. |
| Giant flash | `effect_scale` and muzzle sprite dimensions. |
| UI text overflows | `TextFitted` width/height and logical rect. |
| Click and visual disagree | Use the same rect helper for draw and hit-test. |
| Text missing | Font atlas/pixel font flag; `has_pixel_font`. |
| Black screen | Vulkan/swapchain/asset-root load failure; read startup log. |
| Wrong sprite | Sprite path-ID collision or stale runtime asset. |
| Wrong depth | sorting layer/order, `ground_depth`, tie-break. |
