#include "BoxSystem.hpp"

#include <algorithm>
#include <cmath>

namespace soulknight::game::items {
namespace {
constexpr int kEnergyValue = 8;      // recovered RGEnergy.value
constexpr float kEnergySpeed = 14.0f; // recovered RGEnergy.speed
constexpr float kEnergyRange = 6.0f;  // recovered RGEnergy.range
} // namespace

void BoxSystem::Configure(const engine::assets::SceneAsset& scene,
                          const room::ObstacleField& obstacles) {
    boxes_.clear();
    bubbles_.clear();
    for (std::size_t i = 0; i < obstacles.Markers().size(); ++i) {
        const engine::assets::ObstacleMarker& marker = obstacles.Markers()[i];
        const engine::assets::ObstacleVariant& variant =
            obstacles.VariantForMarker(marker);
        if (!variant.breakable || variant.hp <= 0) continue;
        BreakableBox box;
        box.marker_index = i;
        box.position = marker.position;
        box.hp = variant.hp;
        box.max_hp = variant.hp;
        box.energy_pot = variant.name.rfind("energy_pot", 0) == 0;
        boxes_.push_back(std::move(box));
    }
    (void)scene;
}

void BoxSystem::Reset() {
    boxes_.clear();
    bubbles_.clear();
}

bool BoxSystem::DeployBox(const engine::Vec2& position, int hp,
                          int32_t sprite_index) {
    if (hp <= 0) return false;
    BreakableBox box;
    box.marker_index = BreakableBox::kDeployedMarker;
    box.position = position;
    box.sprite_index = sprite_index;
    box.hp = hp;
    box.max_hp = hp;
    boxes_.push_back(box);
    return true;
}

void BoxSystem::AddColliders(
        engine::collision::AabbCollisionWorld& world) const {
    for (const BreakableBox& box : boxes_) {
        if (box.alive) world.AddSolid(BoxBounds(box));
    }
}

engine::collision::Aabb BoxSystem::BoxBounds(const BreakableBox& box) {
    return {{box.position.x - 0.45f, box.position.y + 0.10f},
            {box.position.x + 0.45f, box.position.y + 1.10f}};
}

bool BoxSystem::Damage(std::size_t index, int damage) {
    if (index >= boxes_.size()) return false;
    BreakableBox& box = boxes_[index];
    if (!box.alive || damage <= 0) return false;
    box.hp -= damage;
    box.hit_flash = 0.10f;
    if (box.hp > 0) return true;
    box.alive = false;
    // RGPetController-style energy pickup: RGEnergy value 8. The reference
    // RGBox.CreateItem normal path instantiates PrefabManager.GetPrefab(8);
    // the recovered RGEnergy register prefab is the resolved pickup written
    // here. The buff-19 branch has an additional 10% roll for the same pickup.
    SpawnBubble(box.position + engine::Vec2{0.0f, 0.7f});
    return true;
}

int BoxSystem::ApplyMelee(const engine::Vec2& origin,
                          const engine::Vec2& direction,
                          float range, float arc_degrees, int damage) {
    constexpr float kPi = 3.14159265358979323846f;
    const engine::Vec2 aim = direction.LengthSquared() > 1e-8f
        ? direction.Normalized() : engine::Vec2{1.0f, 0.0f};
    const float half_arc = std::clamp(arc_degrees, 1.0f, 360.0f) * 0.5f;
    int hits = 0;
    for (std::size_t i = 0; i < boxes_.size(); ++i) {
        const BreakableBox& box = boxes_[i];
        if (!box.alive) continue;
        const engine::Vec2 offset = box.position +
            engine::Vec2{0.0f, 0.55f} - origin;
        const float distance = offset.Length();
        if (distance > range + 0.7f) continue;
        if (distance > 1e-4f) {
            const float angle = std::atan2(offset.y, offset.x) * 180.0f / kPi;
            const float aim_angle = std::atan2(aim.y, aim.x) * 180.0f / kPi;
            float delta = std::fabs(angle - aim_angle);
            if (delta > 180.0f) delta = 360.0f - delta;
            if (delta > half_arc) continue;
        }
        if (Damage(i, damage)) ++hits;
    }
    return hits;
}

int BoxSystem::ApplyProjectiles(combat::ProjectileSystem& projectiles,
                                int player_camp) {
    int hits = 0;
    for (combat::Projectile& projectile : projectiles.Projectiles()) {
        if (!projectile.active || projectile.camp != player_camp) continue;
        const engine::collision::Aabb bullet{
            {projectile.position.x - projectile.collider_half_extents.x,
             projectile.position.y - projectile.collider_half_extents.y},
            {projectile.position.x + projectile.collider_half_extents.x,
             projectile.position.y + projectile.collider_half_extents.y}};
        for (std::size_t i = 0; i < boxes_.size(); ++i) {
            if (!boxes_[i].alive) continue;
            if (!bullet.Intersects(BoxBounds(boxes_[i]))) continue;
            projectile.active = false;
            if (Damage(i, projectile.damage)) ++hits;
            break;
        }
    }
    return hits;
}

void BoxSystem::SpawnBubble(const engine::Vec2& position) {
    EnergyBubble bubble;
    bubble.position = position;
    bubble.velocity = {0.0f, 2.2f};
    bubbles_.push_back(std::move(bubble));
}

int BoxSystem::Update(float delta_seconds,
                      const engine::Vec2& player_position) {
    if (delta_seconds <= 0.0f) return 0;
    for (BreakableBox& box : boxes_) {
        box.hit_flash = std::max(0.0f, box.hit_flash - delta_seconds);
    }
    int absorbed = 0;
    for (EnergyBubble& bubble : bubbles_) {
        if (!bubble.active) continue;
        bubble.age += delta_seconds;
        const engine::Vec2 delta = player_position - bubble.position;
        const float distance = delta.Length();
        if (distance < 0.55f) {
            bubble.active = false;
            absorbed += kEnergyValue;
            continue;
        }
        if (distance < kEnergyRange && distance > 0.01f) {
            bubble.velocity += delta.Normalized() *
                (kEnergySpeed * 0.65f * delta_seconds);
            bubble.velocity *= std::max(0.0f, 1.0f - 2.2f * delta_seconds);
        } else {
            bubble.velocity.y -= 3.0f * delta_seconds;
            bubble.velocity.x *= std::max(0.0f, 1.0f - 2.0f * delta_seconds);
        }
        bubble.position += bubble.velocity * delta_seconds;
    }
    return absorbed;
}

} // namespace soulknight::game::items
