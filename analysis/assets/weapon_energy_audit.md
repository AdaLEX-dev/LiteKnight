# Weapon Energy Cost Audit

| Weapon | Damage | Cycle | Energy cost | Projectile |
|---|---:|---:|---:|---|
| weapon_000 (Bad Pistol) | 3 | 0.3333333432674408 | 0 | bullet_0 |
| weapon_002 (AK-47) | 3 | 0.1666666716337204 | 1 | bullet_14 |
| weapon_048 (Next-gen SMG) | 3 | 0.5 | 1 | bullet_11 |
| weapon_287 (Revolver Gold) | 4 | 0.13333334028720856 | 1 | bullet_14 |

Serialized `consume` is the per-attack energy cost, confirmed by `RGWeapon.MakeConsume` and `MultiHand.CheckEnergyEnough`. No separate ammo cost is claimed.
