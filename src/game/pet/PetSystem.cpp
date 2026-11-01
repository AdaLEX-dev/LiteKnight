#include "PetSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace soulknight::game::pet {
namespace {
constexpr float kPi = 3.14159265358979323846f;

float Distance(const engine::Vec2& a, const engine::Vec2& b) {
    return (b - a).Length();
}
} // namespace

bool PetSystem::Configure(const engine::assets::SceneAsset& scene,
                          std::string* error) {
    definitions_ = scene.pet_definitions;
    if (definitions_.empty()) {
        selected_ = 0;
        selected_id_.clear();
        spawned_ = false;
        if (error != nullptr) *error = "scene has no recovered pet definitions";
        return false;
    }
    std::size_t selected = 0;
    // Reference ui_chose default: emPet.Cat (index 0). The prefab id is "pet".
    for (std::size_t i = 0; i < definitions_.size(); ++i) {
        if (definitions_[i].id == "pet") {
            selected = i;
            break;
        }
    }
    if (!selected_id_.empty()) {
        const auto it = std::find_if(
            definitions_.begin(), definitions_.end(),
            [&](const engine::assets::PetDefinition& definition) {
                return definition.id == selected_id_;
            });
        if (it != definitions_.end()) {
            selected = static_cast<std::size_t>(it - definitions_.begin());
        }
    }
    selected_ = std::min(selected, definitions_.size() - 1);
    selected_id_ = definitions_[selected_].id;
    spawned_ = false;
    attack_cooldown_ = 0.0f;
    attack_animation_remaining_ = 0.0f;
    animation_time_ = 0.0f;
    return true;
}

void PetSystem::Reset() {
    spawned_ = false;
    attack_cooldown_ = 0.0f;
    attack_animation_remaining_ = 0.0f;
    animation_time_ = 0.0f;
    moving_ = false;
}

const engine::assets::PetDefinition* PetSystem::Selected() const {
    return Definition();
}

const engine::assets::PetDefinition* PetSystem::Definition() const {
    if (selected_ >= definitions_.size()) return nullptr;
    return &definitions_[selected_];
}

bool PetSystem::Select(std::size_t index) {
    if (index >= definitions_.size()) return false;
    selected_ = index;
    selected_id_ = definitions_[index].id;
    spawned_ = false;
    return true;
}

bool PetSystem::SelectById(const std::string& id) {
    for (std::size_t i = 0; i < definitions_.size(); ++i) {
        if (definitions_[i].id == id) return Select(i);
    }
    return false;
}

void PetSystem::SelectSaved(const std::string& id) {
    if (id.empty() || !SelectById(id)) {
        for (std::size_t i = 0; i < definitions_.size(); ++i) {
            if (definitions_[i].id == "pet") {
                selected_ = i;
                selected_id_ = definitions_[i].id;
                return;
            }
        }
        if (!definitions_.empty()) {
            selected_ = 0;
            selected_id_ = definitions_.front().id;
        }
    }
}

std::size_t PetSystem::Update(float delta_seconds,
                              const engine::Vec2& player_position,
                              bool player_facing_left,
                              enemy::EnemySystem& enemies,
                              const engine::collision::AabbCollisionWorld* world) {
    const engine::assets::PetDefinition* definition = Definition();
    if (definition == nullptr || delta_seconds <= 0.0f) return definitions_.size();
    if (!spawned_) {
        position_ = player_position +
            engine::Vec2{player_facing_left ? 1.15f : -1.15f, 0.0f};
        spawned_ = true;
    }
    animation_time_ += delta_seconds;
    attack_cooldown_ = std::max(0.0f, attack_cooldown_ - delta_seconds);
    attack_animation_remaining_ =
        std::max(0.0f, attack_animation_remaining_ - delta_seconds);

    // Scout: nearest living enemy within the recovered min_distance. This is
    // the same target selection role as RGPetController.Scout; no wall
    // pathfinding is invented beyond axis-separated collision sliding.
    std::size_t target = definitions_.size();
    float target_distance = std::numeric_limits<float>::max();
    float scout_distance = std::max(definition->scout_distance,
                                    definition->atk_range);
    for (std::size_t i = 0; i < enemies.Enemies().size(); ++i) {
        const enemy::EnemyInstance& candidate = enemies.Enemies()[i];
        if (!candidate.active || candidate.health.dead) continue;
        const float distance = Distance(position_, candidate.position);
        if (distance <= scout_distance && distance < target_distance) {
            target = i;
            target_distance = distance;
        }
    }

    engine::Vec2 destination = player_position;
    float stop_distance = definition->min_follow_distance;
    if (target < definitions_.size()) {
        destination = enemies.Enemies()[target].position;
        stop_distance = definition->atk_range;
    }
    const float distance = Distance(position_, destination);
    moving_ = false;
    if (distance > definition->max_follow_distance) {
        // Recovered RGPetController.FixedUpdate teleports when farther than
        // max_follow_distance.
        position_ = destination;
    } else if (distance > stop_distance + 0.05f) {
        const engine::Vec2 direction = (destination - position_) *
                                       (1.0f / std::max(distance, 0.0001f));
        const float speed = definition->speed *
                            (1.0f + definition->speed_rate);
        const engine::Vec2 delta = direction * speed * delta_seconds;
        if (world != nullptr) {
            engine::Vec2 resolved = position_;
            (void)world->Move(position_, {0.30f, 0.30f}, delta, resolved);
            position_ = resolved;
        } else {
            position_ += delta;
        }
        if (std::fabs(direction.x) > 0.01f) {
            facing_left_ = direction.x < 0.0f;
        }
        moving_ = true;
    }

    if (target < definitions_.size() &&
        target_distance <= definition->atk_range + 0.20f &&
        attack_cooldown_ <= 0.0f) {
        const bool hit = enemies.ApplyDamage(target, definition->damage, 1);
        attack_cooldown_ = std::max(0.05f, definition->atk_cd);
        attack_animation_remaining_ = 0.30f;
        if (hit) return target;
    }
    return definitions_.size();
}

int32_t PetSystem::SampleFrames(
        const std::vector<engine::assets::PetFrame>& frames) const {
    if (frames.empty()) return -1;
    const float duration = std::max(0.1f, frames.back().time + 0.1f);
    const float time = std::fmod(animation_time_, duration);
    int32_t sprite = frames.front().sprite_index;
    for (const engine::assets::PetFrame& frame : frames) {
        if (frame.time > time) break;
        sprite = frame.sprite_index;
    }
    return sprite;
}

int32_t PetSystem::CurrentSprite() const {
    const engine::assets::PetDefinition* definition = Definition();
    if (definition == nullptr) return -1;
    if (attack_animation_remaining_ > 0.0f) {
        const float elapsed = 0.30f - attack_animation_remaining_;
        if (!definition->attack_frames.empty()) {
            int32_t sprite = definition->attack_frames.front().sprite_index;
            for (const engine::assets::PetFrame& frame :
                 definition->attack_frames) {
                if (frame.time > elapsed) break;
                sprite = frame.sprite_index;
            }
            return sprite;
        }
    }
    if (moving_ && !definition->run_frames.empty()) {
        return SampleFrames(definition->run_frames);
    }
    if (!definition->idle_frames.empty()) {
        return SampleFrames(definition->idle_frames);
    }
    return definition->body_sprite_index;
}

int32_t PetSystem::ShadowSprite() const {
    const engine::assets::PetDefinition* definition = Definition();
    return definition != nullptr ? definition->shadow_sprite_index : -1;
}

} // namespace soulknight::game::pet
