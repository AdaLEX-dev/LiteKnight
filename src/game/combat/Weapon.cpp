// Soul Knight Linux — recovered weapon fire path
#include "Weapon.hpp"

#include <algorithm>
#include <cmath>

namespace soulknight::game::combat {
using engine::assets::WeaponProjectileVariant;
namespace {

constexpr float kPi = 3.14159265358979323846f;

Vec2 RotateDegrees(const Vec2& direction, float degrees) {
    const float radians = degrees * kPi / 180.0f;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {direction.x * c - direction.y * s,
            direction.x * s + direction.y * c};
}

float DirectionDegrees(const Vec2& direction) {
    return std::atan2(direction.y, direction.x) * 180.0f / kPi;
}

} // namespace

bool Weapon::Configure(const engine::assets::WeaponDefinition& definition) {
    definition_ = definition;
    attack_time_ = 0.0f;
    next_shot_time_ = 0.0f;
    attacking_ = false;
    configured_ = (definition_.is_melee ||
                   !definition_.special.kind.empty() ||
                   (definition_.bullet_speed > 0.0f &&
                    definition_.projectile.lifetime > 0.0f)) &&
                  definition_.attack_cycle_seconds > 0.0f &&
                  definition_.first_shot_delay_seconds >= 0.0f;
    // Native deterministic test seed. The reference weapon owns an RGRandom
    // stream; the exact runtime seed is not serialized in the prefab.
    random_.SetRandomSeed(1);
    speed_multiplier_ = 1.0f;
    return configured_;
}

void Weapon::SetAttackSpeedMultiplier(float value) {
    speed_multiplier_ = std::isfinite(value)
        ? std::clamp(value, 0.1f, 10.0f) : 1.0f;
}

void Weapon::Clear() {
    definition_ = {};
    configured_ = false;
    Release();
}

void Weapon::Release() {
    attacking_ = false;
    attack_time_ = 0.0f;
    cycle_start_time_ = 0.0f;
    next_shot_time_ = 0.0f;
    event_index_ = 0;
    fire_events_.clear();
}

int Weapon::Update(float delta_seconds,
                   bool attack_held,
                   const Vec2& owner_position,
                   const Vec2& aim_direction,
                   ProjectileSystem& projectiles,
                   player::EnergyState* energy) {
    if (!configured_ || delta_seconds <= 0.0f || !std::isfinite(delta_seconds)) {
        return 0;
    }
    if (!attack_held) {
        Release();
        return 0;
    }
    if (!attacking_) {
        attacking_ = true;
        attack_time_ = 0.0f;
        cycle_start_time_ = 0.0f;
        event_index_ = 0;
        next_shot_time_ = definition_.attack_event_times.empty()
            ? definition_.first_shot_delay_seconds
            : definition_.attack_event_times.front();
    }

    attack_time_ += delta_seconds * speed_multiplier_;
    int fired = 0;
    // The recovered attack states loop and may carry several Attack events per
    // loop (Assault Sniper Rifle: 0.1/0.4/0.7667 s; Gun008: code-driven burst).
    // A long render frame can legitimately cross more than one event in a
    // single fixed tick; preserve every event rather than dropping one.
    constexpr float kEventEpsilon = 1e-6f;
    constexpr int kMaxEventsPerTick = 64;
    while (attack_time_ + kEventEpsilon >= next_shot_time_ &&
           fired < kMaxEventsPerTick) {
        const int cost = definition_.EnergyCost();
        if (energy != nullptr && cost > 0 && !energy->CanConsume(cost)) {
            // RGWeapon.SetAttack rejects the attack path when the controller's
            // energy is below the serialized consume value. Stop this cycle
            // until energy is restored by a recovered source.
            Release();
            break;
        }
        bool spawn_projectile = !definition_.is_melee;
        const std::string& special_kind = definition_.special.kind;
        const bool projectile_special =
            special_kind == "multi_pattern" ||
            special_kind == "revolver_burst" ||
            special_kind == "invisible_missile";
        const bool passive = special_kind == "passive" ||
                             special_kind == "revive";
        if (passive) {
            spawn_projectile = false;
        } else if (!special_kind.empty() && !projectile_special) {
            // Subclass visuals/effects are driven by the gameplay layer; the
            // weapon still owns the recovered animation cadence and energy
            // accounting. Bayonet Rifle also fires its rifle projectile.
            fire_events_.push_back(WeaponFireEvent{
                special_kind, owner_position,
                aim_direction.LengthSquared() > 1e-8f
                    ? aim_direction.Normalized() : Vec2{1.0f, 0.0f},
                static_cast<int>(event_index_)});
            spawn_projectile = special_kind == "bayonet" ||
                               special_kind == "eatable_throw";
        } else if (projectile_special) {
            spawn_projectile = true;
        }
        if (spawn_projectile) {
            SpawnProjectile(owner_position, aim_direction, projectiles);
        }
        if (energy != nullptr && cost > 0) {
            (void)energy->Consume(cost);
        }
        ++fired;
        ++event_index_;
        if (definition_.attack_event_times.empty()) {
            next_shot_time_ += definition_.attack_cycle_seconds;
        } else if (event_index_ < definition_.attack_event_times.size()) {
            next_shot_time_ = cycle_start_time_ +
                definition_.attack_event_times[event_index_];
        } else {
            cycle_start_time_ += definition_.attack_cycle_seconds;
            event_index_ = 0;
            next_shot_time_ = cycle_start_time_ +
                definition_.attack_event_times.front();
        }
    }
    return fired;
}

Vec2 Weapon::MuzzlePosition(const Vec2& owner_position,
                            const Vec2& aim_direction) const {
    Vec2 aim = aim_direction;
    if (aim.LengthSquared() < 1e-8f) aim = {1.0f, 0.0f};
    aim = aim.Normalized();
    const Vec2 hand_offset{0.0f, 0.5f};
    const Vec2 gun_point = RotateDegrees(definition_.muzzle_offset,
                                         DirectionDegrees(aim));
    return owner_position + hand_offset + gun_point;
}

void Weapon::SpawnProjectile(const Vec2& owner_position,
                             const Vec2& aim_direction,
                             ProjectileSystem& projectiles) {
    Vec2 aim = aim_direction;
    if (aim.LengthSquared() < 1e-8f) aim = {1.0f, 0.0f};
    aim = aim.Normalized();

    // Gun001.CreateBullet: the recovered arithmetic is
    //   spread = attribute.deviation * weapon.deviation + attribute.deviation
    //          = attribute.deviation * (weapon.deviation + 1)
    // c00's serialized RoleAttribute has no deviation field, and
    // RoleAttribute's constructor leaves it at 0, so weapon_000's effective
    // spread is 0 degrees in the current dev-room player case.
    // GunMultiBullet02 / Rainbow cycles recovered elemental bullet prefabs
    // with the serialized angle step (i - multiCount/2) * angle.
    if (definition_.special.kind == "multi_pattern" &&
        !definition_.special.variants.empty()) {
        const int count = std::max(1, definition_.special.count);
        const float angle = definition_.special.value;
        for (int i = 0; i < count; ++i) {
            const WeaponProjectileVariant& variant =
                definition_.special.variants[
                    static_cast<std::size_t>(i) %
                    definition_.special.variants.size()];
            const float offset = (static_cast<float>(i) -
                                  static_cast<float>(count) * 0.5f) * angle;
            const Vec2 direction = RotateDegrees(aim, offset);
            projectiles.Spawn(variant.projectile, MuzzlePosition(
                    owner_position, direction), direction,
                variant.bullet_speed, variant.damage,
                variant.projectile.camp, variant.through, variant.repel);
        }
        return;
    }
    // Gun008 alternates its two recovered bullet entries across the burst.
    if (definition_.special.kind == "revolver_burst" &&
        !definition_.special.variants.empty()) {
        const WeaponProjectileVariant& variant =
            definition_.special.variants[
                static_cast<std::size_t>(fire_events_.size() +
                                         event_index_) %
                definition_.special.variants.size()];
        projectiles.Spawn(variant.projectile,
                          MuzzlePosition(owner_position, aim), aim,
                          variant.bullet_speed, variant.damage,
                          variant.projectile.camp, variant.through,
                          variant.repel);
        return;
    }
    const float spread = static_cast<float>(definition_.owner_deviation) *
                         (static_cast<float>(definition_.deviation) + 1.0f);
    const int count = std::max(1, definition_.bullet_count);
    const float multi_spread = definition_.spread_degrees > 0
        ? static_cast<float>(definition_.spread_degrees)
        : (count > 1 ? static_cast<float>(definition_.deviation) : 0.0f);
    // APPROXIMATION: the recovered muzzle is composed through the player hand
    // and weapon gun_point transforms. c00 exposes hand `h1` local (0, 0.5);
    // each weapon exposes its serialized gun_point local offset. The exact
    // animator-driven hand transform is not modeled.
    const Vec2 muzzle = MuzzlePosition(owner_position, aim);
    for (int i = 0; i < count; ++i) {
        const float offset_degrees = count > 1 && multi_spread > 0.0f
            ? random_.Range(-multi_spread * 0.5f, multi_spread * 0.5f)
            : (spread > 0.0f ? random_.Range(-spread, spread) : 0.0f);
        const Vec2 direction = RotateDegrees(aim, offset_degrees);
        projectiles.Spawn(definition_.projectile, muzzle, direction,
                          definition_.bullet_speed, definition_.damage,
                          definition_.projectile.camp, definition_.through,
                          definition_.repel);
    }
}

} // namespace soulknight::game::combat
