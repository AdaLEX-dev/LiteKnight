// Soul Knight Linux — recovered weapon fire path
#pragma once

#include "Damage.hpp"
#include "ProjectileSystem.hpp"
#include "engine/assets/SceneAsset.hpp"
#include "engine/math/UnityRandom.hpp"
#include "game/player/Energy.hpp"
#include "engine/math/Vec2.hpp"

namespace soulknight::game::combat {

using engine::Vec2;

// Native subset of the recovered RGWeapon/Gun001 attack cycle:
//
//   atk_b (attack held) -> weapon Animator state `w_pistol`
//   `w_pistol` loops for 0.333333 s with an `Attack` animation event at
//   0.033333 s -> Gun001.Attack -> CreateBullet
//
// weapon_000 has weapon_speed = 1.0 and the controller atk-speed factor is
// 1.0, so the recovered cycle is one shot every 1/3 s while attack is held.
// The `Attack` event is the first shot 1/30 s after entering the state.
// Random spread uses the weapon's own RGRandom stream; the prefab does not
// serialize its runtime seed, so the native stream seed is a test binding.
class Weapon {
public:
    bool Configure(const engine::assets::WeaponDefinition& definition);
    void Clear();
    bool IsConfigured() const { return configured_; }

    // Advances the attack state and spawns projectiles for any Attack events
    // crossed by this fixed tick. Returns the number of projectiles created.
    int Update(float delta_seconds,
               bool attack_held,
               const Vec2& owner_position,
               const Vec2& aim_direction,
               ProjectileSystem& projectiles,
               player::EnergyState* energy = nullptr);

    // Stops the attack cycle without discarding the current cooldown state.
    void Release();

    const engine::assets::WeaponDefinition& Definition() const {
        return definition_;
    }
    // Same hand + gun_point composition used by SpawnProjectile, exposed for
    // the F4 aim/muzzle debug overlay.
    Vec2 MuzzlePosition(const Vec2& owner_position,
                        const Vec2& aim_direction) const;
    bool IsAttacking() const { return attacking_; }
    float AttackTime() const { return attack_time_; }

private:
    void SpawnProjectile(const Vec2& owner_position,
                         const Vec2& aim_direction,
                         ProjectileSystem& projectiles);

    engine::assets::WeaponDefinition definition_{};
    engine::RGRandomStream random_;
    float attack_time_ = 0.0f;
    float next_shot_time_ = 0.0f;
    bool attacking_ = false;
    bool configured_ = false;
};

} // namespace soulknight::game::combat
