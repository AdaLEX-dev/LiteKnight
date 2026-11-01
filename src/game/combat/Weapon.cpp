// Soul Knight Linux — recovered weapon fire path
#include "Weapon.hpp"

#include <algorithm>
#include <cmath>

namespace soulknight::game::combat {
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
    configured_ = definition_.bullet_speed > 0.0f &&
                  definition_.projectile.lifetime > 0.0f &&
                  definition_.attack_cycle_seconds > 0.0f &&
                  definition_.first_shot_delay_seconds >= 0.0f;
    // Native deterministic test seed. The reference weapon owns an RGRandom
    // stream; the exact runtime seed is not serialized in the prefab.
    random_.SetRandomSeed(1);
    return configured_;
}

void Weapon::Clear() {
    definition_ = {};
    configured_ = false;
    Release();
}

void Weapon::Release() {
    attacking_ = false;
    attack_time_ = 0.0f;
    next_shot_time_ = 0.0f;
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
        next_shot_time_ = definition_.first_shot_delay_seconds;
    }

    attack_time_ += delta_seconds;
    int fired = 0;
    // The recovered `w_pistol` state loops, and each loop emits one Attack
    // event. A long render frame can therefore legitimately cross more than
    // one event in a single fixed tick; preserve every shot rather than
    // dropping one.
    constexpr float kEventEpsilon = 1e-6f;
    while (attack_time_ + kEventEpsilon >= next_shot_time_) {
        const int cost = definition_.EnergyCost();
        if (energy != nullptr && cost > 0 && !energy->CanConsume(cost)) {
            // RGWeapon.SetAttack rejects the attack path when the controller's
            // energy is below the serialized consume value. Stop this cycle
            // until energy is restored by a recovered source.
            Release();
            break;
        }
        SpawnProjectile(owner_position, aim_direction, projectiles);
        if (energy != nullptr && cost > 0) {
            (void)energy->Consume(cost);
        }
        ++fired;
        next_shot_time_ += definition_.attack_cycle_seconds;
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
