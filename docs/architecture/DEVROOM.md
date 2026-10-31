# Devroom Architecture

## Runtime flow

```text
Application
  -> Window / InputManager (SDL3, Wayland)
  -> FrameTimer (50 Hz fixed ticks)
  -> PlayerLocomotion + AabbCollision
  -> WeaponRegistry -> Weapon -> ProjectileSystem
  -> EnemySystem (fixtures + developer spawner)
  -> HealthState (player/enemy damage)
  -> Camera2D
  -> SpriteBatch -> SpriteRenderer (Vulkan)
  -> DevOverlay / DebugTextRenderer (UI layer)
```

## Data/UI separation

| Concept | Existing type |
|---|---|
| CharacterDefinition | `engine::assets::SceneCharacter` |
| CharacterRegistry | `game::player::PlayerCharacterSet` |
| Character selector | `game::ui::DevOverlay` character mode |
| CharacterInstance | active `PlayerLocomotion` + active SceneCharacter |
| WeaponDefinition | `engine::assets::WeaponDefinition` |
| WeaponInstance | `game::combat::Weapon` |
| WeaponRegistry | `game::weapons::WeaponRegistry` |
| WeaponRenderer | `game::weapons::WeaponRenderer` |
| ProjectileDefinition | `engine::assets::ProjectileDefinition` |
| ProjectileInstance | `game::combat::Projectile` |
| EnemyDefinition | `engine::assets::EnemyDefinition` |
| EnemyRegistry/spawner | `game::enemy::EnemySystem` |
| Developer text | `engine::rendering::DebugTextRenderer` |

The names above are deliberately mapped to the existing project rather than
introducing duplicate wrapper types. Selection state lives in `DevOverlay`;
gameplay state remains in the existing simulation objects.

## Rendering contract

- Player body position is resolved by `PlayerRenderSync`; only the recovered
  `(0,-0.05)` shadow offset is allowed.
- Projectile draw position is exactly `Projectile.position`.
- Enemy draw position is exactly `EnemyInstance.position`.
- Weapon visual is drawn from the recovered prefab sprite by `WeaponRenderer`.
- Developer UI is drawn through the same `SpriteBatch`/Vulkan path on the UI
  sorting layer.
- The developer font atlas is a generated PLACEHOLDER; no proprietary UI font
  is claimed.

## Input contract

Input is sampled once per render frame and consumed by fixed steps. Selectors
capture navigation keys and suppress movement/attack while open. Legacy debug
shortcuts remain available when selectors are closed.
