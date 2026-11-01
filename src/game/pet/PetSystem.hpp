// Soul Knight Linux — recovered RGPetController follower runtime
#pragma once

#include "engine/assets/SceneAsset.hpp"
#include "engine/collision/AabbCollision.hpp"
#include "engine/math/Vec2.hpp"
#include "game/enemy/EnemySystem.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace soulknight::game::pet {

// Native subset of the recovered RGPetController family. The reference
// controller follows its master, teleports beyond max_follow_distance, stops
// inside min_follow_distance, scopes a target within min_distance and attacks
// on atk_cd. Pet-weapon projectiles are not spawned here; pets with recovered
// atk_range use the same frontal contact query for the shared attack window.
class PetSystem {
public:
    bool Configure(const engine::assets::SceneAsset& scene, std::string* error);
    void Reset();

    std::size_t Count() const { return definitions_.size(); }
    const std::vector<engine::assets::PetDefinition>& Definitions() const {
        return definitions_;
    }
    std::size_t SelectedIndex() const { return selected_; }
    const engine::assets::PetDefinition* Selected() const;
    const std::string& SelectedId() const { return selected_id_; }

    bool Select(std::size_t index);
    bool SelectById(const std::string& id);
    // Restores a saved pet by index; unknown ids keep the first recovered pet.
    void SelectSaved(const std::string& id);

    // Follow/attack update. Returns the damaged enemy index or Count() when no
    // enemy was hit this tick.
    std::size_t Update(float delta_seconds, const engine::Vec2& player_position,
                       bool player_facing_left, enemy::EnemySystem& enemies,
                       const engine::collision::AabbCollisionWorld* world);

    engine::Vec2 Position() const { return position_; }
    bool FacingLeft() const { return facing_left_; }
    bool Moving() const { return moving_; }
    bool Attacking() const { return attack_animation_remaining_ > 0.0f; }
    // Sprite for the current animation state, or -1 when no pet is configured.
    int32_t CurrentSprite() const;
    int32_t ShadowSprite() const;

private:
    const engine::assets::PetDefinition* Definition() const;
    int32_t SampleFrames(const std::vector<engine::assets::PetFrame>& frames) const;

    std::vector<engine::assets::PetDefinition> definitions_;
    std::size_t selected_ = 0;
    std::string selected_id_;
    engine::Vec2 position_{0.0f, 0.0f};
    bool facing_left_ = false;
    bool moving_ = false;
    bool spawned_ = false;
    float animation_time_ = 0.0f;
    float attack_cooldown_ = 0.0f;
    float attack_animation_remaining_ = 0.0f;
};

} // namespace soulknight::game::pet
