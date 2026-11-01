// Soul Knight Linux — recovered weapon fire path
#pragma once

#include "Damage.hpp"
#include "ProjectileSystem.hpp"
#include "engine/assets/SceneAsset.hpp"
#include "engine/math/UnityRandom.hpp"
#include "game/player/Energy.hpp"
#include "engine/math/Vec2.hpp"

#include <cmath>
#include <string>
#include <vector>

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
struct WeaponFireEvent {
    std::string kind;
    Vec2 position{0.0f, 0.0f};
    Vec2 direction{1.0f, 0.0f};
    int index = 0;
};

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

    // BuffAtkSpeedChange and weapon add_atkspeed feed this multiplier. The
    // recovered attack events remain in clip time; advancing clip time faster
    // is the same operation as Animator.speed scaling.
    void SetAttackSpeedMultiplier(float value);

    const engine::assets::WeaponDefinition& Definition() const {
        return definition_;
    }
    // Same hand + gun_point composition used by SpawnProjectile, exposed for
    // the F4 aim/muzzle debug overlay.
    Vec2 MuzzlePosition(const Vec2& owner_position,
                        const Vec2& aim_direction) const;
    // Serialized subclass effects (summon, buff, deployable, full-screen,
    // death mark, revive) are consumed by the gameplay layer after Update.
    std::vector<WeaponFireEvent> TakeFireEvents() {
        std::vector<WeaponFireEvent> events;
        events.swap(fire_events_);
        return events;
    }
    bool IsAttacking() const { return attacking_; }
    bool IsCharging() const { return charging_; }
    float ChargePercent() const {
        return definition_.special.duration > 0.0f
            ? std::clamp(charge_time_ / definition_.special.duration, 0.0f, 1.0f)
            : 0.0f;
    }
    float AttackTime() const { return attack_time_; }
    float VisualTime() const {
        if (!attacking_ || definition_.attack_cycle_seconds <= 0.0f) return 0.0f;
        return std::fmod(attack_time_, definition_.attack_cycle_seconds);
    }

private:
    void SpawnProjectile(const Vec2& owner_position,
                         const Vec2& aim_direction,
                         ProjectileSystem& projectiles);
    int UpdateCharge(float delta_seconds, bool attack_held,
                     const Vec2& owner_position, const Vec2& aim_direction,
                     ProjectileSystem& projectiles,
                     player::EnergyState* energy);
    int FireCharged(float percent, const Vec2& owner_position,
                    const Vec2& aim_direction,
                    ProjectileSystem& projectiles,
                    player::EnergyState* energy);
    Vec2 RotateAim(const Vec2& aim, float degrees) const;

    engine::assets::WeaponDefinition definition_{};
    engine::RGRandomStream random_;
    float attack_time_ = 0.0f;
    float cycle_start_time_ = 0.0f;
    float next_shot_time_ = 0.0f;
    std::size_t event_index_ = 0;
    bool attacking_ = false;
    bool configured_ = false;
    bool charging_ = false;
    float charge_time_ = 0.0f;
    float charge_cooldown_ = 0.0f;
    float end_shot_charge_ = 0.0f;
    bool end_shot_was_held_ = false;
    float speed_multiplier_ = 1.0f;
    std::vector<WeaponFireEvent> fire_events_;
};

} // namespace soulknight::game::combat
