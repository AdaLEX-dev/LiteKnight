# Special Room Interactions

## Recovered systems

| Room | System | Recovered behavior | Status |
|---|---|---|---|
| `r_statue` | `TalkStatue` | 9 statue prefabs, base price 15, two-press purchase, persistent statue id | **WIRED (native reconstruction)** |
| `r_mineral` | `EnemyAI05` minerals | 40 HP, 10 pickups: gold drops 10×`coin_2`, crystal drops 10×`energy` | **WIRED (E-hit reconstruction)** |
| `r_mineral` | water dispenser | free heal 1–4 | **RECOVERED, NOT WIRED** |
| `r_mineral` | bar/waiter | drinks 20–30 | **RECOVERED, NOT WIRED** |
| `r_mercenary` | hiring/trainer | `npc_01..06` 15-coin hire, trainer 10/20/40/80 to level 5 | **WIRED (follower reconstruction)** |
| `r_mount` | mount seller | creature/mech 15/30, mounted speed/dismount | **WIRED (status/speed reconstruction)** |

Exact fields, addresses and pools are recorded in the recovery report and in
`analysis/assets/chests/chest_catalogue.md`.

## Native reconstruction

The random `enemy_group` variants are Odin-serialized, so the native runtime
chooses one recovered variant deterministically per room seed and places it at
`scene.spawn + (0, 2.8)`. Exact recovered fields (price, HP, drop item/count,
statue index, mercenary damage) are used. Positions and the E-hit/E-confirm
interactions are **NATIVE RECONSTRUCTION**; the room geometry is exact.

Implemented:

- Statue: two-press purchase, coin deduction, persistent active statue id.
  The specific `buff_statue_<index>` stat effect is not claimed because its
  runtime fields were not decoded.
- Mineral: 40 HP, each interaction hit removes 10 HP, then spawns exactly 10
  coins (`coin_2`) or 10 energy bubbles.
- Mercenary: 15-coin hire, persistent follower with recovered body sprite and
  damage; trainer costs 10/20/40/80 to level 5.
- Mount: creature/mech 15/30 purchase, speed bonus and dismount; no invented
  mount art.

## Remaining limitations

- Water dispenser and bar/waiter visuals/interactions remain recovered-only.
- Statue buff stat values and mount riding animation are not decoded; the
  purchase/status flow is wired without inventing stats.
- Save/load persists active statue, mercenary, level, mount and pot/shop state.
- Deterministic seller/special-object fields are covered by unit tests; the
  visual smoke is `--area r_statue/r_mineral/r_mercenary/r_mount`.
