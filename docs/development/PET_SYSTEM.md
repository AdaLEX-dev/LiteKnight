# Pet System

## Recovered data

`RGSaveManager` (common bundle, path id `-1733285495109658362`) serializes:

- `pet_list`: `RGPetInfo[]` with `unlock`, `unlock_gem`, `icon`, `skin_list`.
- `LoadLastPetIndex` / `SaveLastPetIndex`.
- `PetUnLock`, `GetPetUISprite`.
- `gems_icon` (`ui_52`).

`RGPetController` serializes follow/attack fields:

```text
role_attribute, damage, move_direction, min_distance,
reply_time1/2, scout_rate, atk_cd, summon, can_atk,
master_tf, min_follow_distance, max_follow_distance
```

`PetHand` and `PetHandAutoSetup` define a pet weapon hand. `WildPetRandomInfo`
and `RGWildController` are separate wild-pet systems.

## Native state

Not yet implemented. The catalogue icons and unlock data are recovered but no
native pet selection, spawn, follow or save field exists yet. Do not confuse this
with the passive `pet` sprite currently present in the HeroRoom scene.

## Next implementation

1. Export the 13+ `RGPetInfo.icon` sprites.
2. Build a pet menu with actual icons and unlock counts.
3. Add a selected-pet save field.
4. Spawn `RGPetController`-equivalent follower using the recovered min/max follow
   distances and `atk_cd`.
