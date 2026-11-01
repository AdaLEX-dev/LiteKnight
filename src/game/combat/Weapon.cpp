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
    charging_ = false;
    charge_time_ = 0.0f;
    charge_cooldown_ = 0.0f;
    end_shot_charge_ = 0.0f;
    end_shot_was_held_ = false;
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
    const std::string& special_kind = definition_.special.kind;
    int end_shot_fired = 0;
    if (special_kind.rfind("charge_", 0) == 0) {
        return UpdateCharge(delta_seconds, attack_held, owner_position,
                            aim_direction, projectiles, energy);
    }
    if (special_kind == "end_shot") {
        if (attack_held) {
            end_shot_charge_ = std::min(
                definition_.special.duration,
                end_shot_charge_ + delta_seconds *
                    definition_.special.hold_speed * speed_multiplier_);
        } else if (end_shot_was_held_ &&
                   end_shot_charge_ + 1e-4f >=
                       definition_.special.duration) {
            end_shot_fired = FireCharged(1.0f, owner_position, aim_direction,
                                         projectiles, energy);
            fire_events_.push_back(WeaponFireEvent{
                "end_shot", owner_position,
                aim_direction.LengthSquared() > 1e-8f
                    ? aim_direction.Normalized() : Vec2{1.0f, 0.0f}, 0});
            end_shot_charge_ = 0.0f;
        }
        end_shot_was_held_ = attack_held;
    }
    if (!attack_held) {
        Release();
        return end_shot_fired;
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
        const bool projectile_special =
            special_kind == "multi_pattern" ||
            special_kind == "revolver_burst" ||
            special_kind == "invisible_missile" ||
            special_kind == "end_shot";
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

int Weapon::UpdateCharge(float delta_seconds, bool attack_held,
                         const Vec2& owner_position,
                         const Vec2& aim_direction,
                         ProjectileSystem& projectiles,
                         player::EnergyState* energy) {
    if (charge_cooldown_ > 0.0f) {
        charge_cooldown_ = std::max(0.0f, charge_cooldown_ - delta_seconds);
    }
    int fired = 0;
    if (!attack_held) {
        if (charging_) {
            fired += FireCharged(ChargePercent(), owner_position,
                                 aim_direction, projectiles, energy);
            fire_events_.push_back(WeaponFireEvent{
                "charge_release", owner_position,
                aim_direction.LengthSquared() > 1e-8f
                    ? aim_direction.Normalized() : Vec2{1.0f, 0.0f}, 0});
            charging_ = false;
            charge_time_ = 0.0f;
            charge_cooldown_ = definition_.special.inactive_time;
        }
        return fired;
    }
    if (!charging_ && charge_cooldown_ <= 0.0f) {
        charging_ = true;
        charge_time_ = 0.0f;
        fire_events_.push_back(WeaponFireEvent{
            "charge_start", owner_position,
            aim_direction.LengthSquared() > 1e-8f
                ? aim_direction.Normalized() : Vec2{1.0f, 0.0f}, 0});
    }
    if (!charging_) return fired;
    charge_time_ = std::min(
        definition_.special.duration,
        charge_time_ + delta_seconds * definition_.special.hold_speed *
            speed_multiplier_);
    if (charge_time_ + 1e-4f >= definition_.special.duration) {
        fired += FireCharged(1.0f, owner_position, aim_direction,
                             projectiles, energy);
        fire_events_.push_back(WeaponFireEvent{
            "charge_full", owner_position,
            aim_direction.LengthSquared() > 1e-8f
                ? aim_direction.Normalized() : Vec2{1.0f, 0.0f}, 0});
        charging_ = false;
        charge_time_ = 0.0f;
        charge_cooldown_ = definition_.special.inactive_time;
    }
    return fired;
}

Vec2 Weapon::RotateAim(const Vec2& aim, float degrees) const {
    return RotateDegrees(aim, degrees);
}

int Weapon::FireCharged(float percent, const Vec2& owner_position,
                        const Vec2& aim_direction,
                        ProjectileSystem& projectiles,
                        player::EnergyState* energy) {
    const float p = std::clamp(percent, 0.0f, 1.0f);
    const int cost = definition_.EnergyCost();
    if (energy != nullptr && cost > 0 && !energy->CanConsume(cost)) {
        return 0;
    }
    Vec2 aim = aim_direction;
    if (aim.LengthSquared() < 1e-8f) aim = {1.0f, 0.0f};
    aim = aim.Normalized();
    const engine::assets::WeaponProjectileVariant* second =
        definition_.special.variants.empty()
            ? nullptr : &definition_.special.variants.front();
    int spawned = 0;
    if (definition_.special.kind == "charge_gun020") {
        const bool full = p >= 1.0f - 1e-4f;
        const engine::assets::ProjectileDefinition& projectile =
            full && second != nullptr ? second->projectile
                                      : definition_.projectile;
        const int damage = definition_.damage +
            (second != nullptr
                 ? static_cast<int>(p * static_cast<float>(second->damage))
                 : 0);
        projectiles.Spawn(projectile, MuzzlePosition(owner_position, aim), aim,
                          definition_.bullet_speed, damage,
                          projectile.camp, definition_.through,
                          definition_.repel);
        ++spawned;
    } else if (definition_.special.kind == "charge_gun007") {
        const int base_count = definition_.special.a_interval > 0.0f
            ? definition_.special.continuous_count
            : definition_.special.multi_count;
        int count = std::max(1, static_cast<int>(
            p * static_cast<float>(base_count)));
        if (definition_.special.multi_count == 0) {
            count = p >= 1.0f - 1e-4f ? 1 : 0;
        }
        for (int i = 0; i < count; ++i) {
            float angle = 0.0f;
            if (definition_.special.a_interval <= 0.0f && count > 1) {
                const float offset = static_cast<float>(i) -
                    static_cast<float>(count) * 0.5f;
                angle = offset * static_cast<float>(
                    definition_.special.angle);
            }
            const Vec2 direction = RotateAim(aim, angle);
            const float speed = definition_.bullet_speed +
                (second != nullptr ? p * second->bullet_speed : 0.0f);
            const int damage = definition_.damage +
                (second != nullptr
                     ? static_cast<int>(p * static_cast<float>(second->damage))
                     : 0);
            const int through = p > 0.6f ? 999 : definition_.through;
            projectiles.Spawn(definition_.projectile,
                              MuzzlePosition(owner_position, direction),
                              direction, speed, damage,
                              definition_.projectile.camp, through,
                              definition_.repel);
            ++spawned;
        }
    } else if (definition_.special.kind == "charge_bow") {
        const float speed = definition_.bullet_speed +
            (second != nullptr ? p * second->bullet_speed : 0.0f);
        const int damage = definition_.damage +
            (second != nullptr
                 ? static_cast<int>(p * static_cast<float>(second->damage))
                 : 0);
        projectiles.Spawn(definition_.projectile,
                          MuzzlePosition(owner_position, aim), aim, speed,
                          damage, definition_.projectile.camp,
                          definition_.through, definition_.repel);
        ++spawned;
    } else if (second != nullptr) {
        projectiles.Spawn(second->projectile,
                          MuzzlePosition(owner_position, aim), aim,
                          second->bullet_speed, second->damage,
                          second->projectile.camp, second->through,
                          second->repel);
        ++spawned;
    }
    if (spawned > 0 && energy != nullptr && cost > 0) {
        (void)energy->Consume(cost);
    }
    return spawned;
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
