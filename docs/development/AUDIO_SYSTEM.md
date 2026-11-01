# Audio System

## Audit

AudioClips were searched in every reference bundle. The first-pass mapping
exported **11 actual Unity clips** through `tools/asset/build_audio_assets.py`:

| logical event | reference clip |
|---|---|
| `ui_click` / `pause` | `fx_tapClick` |
| `pickup` | `fx_pickup` |
| `chest_open` | `fx_chest_open` |
| `weapon_fire` | `fx_gun_1` |
| `player_hit` / `enemy_hit` | `fx_hit` |
| `door` / `transition` | `fx_door` |
| `bgm_hub` | `bgm_room` |
| `bgm_dungeon` | `bgm_1Low` |

Clip metadata (duration/channels/frequency) is committed in
`analysis/assets/audio/audio_catalogue.json`. WAV bytes remain local-only.

## Native engine

The existing miniaudio wrapper (`platform/audio/AudioEngine`) is now initialized
at application startup. It exposes Master/Music/SFX/UI/Ambience buses. The
runtime wires:

- UI click on menu/action input,
- pause/resume sound,
- pickup, chest open, weapon fire, player hit, door/transition,
- looping hub and dungeon music.

If no audio device is available, the application logs a warning and continues.

## Remaining

- Full AudioClip catalogue and 3D/compression metadata audit.
- Per-weapon `RGWeapon.audio_clip` rather than the generic `fx_gun_1`.
- Enemy-specific hit/death sounds.
- Music transitions and volume settings persistence.
