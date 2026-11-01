// Soul Knight Linux — recovered EnemyAIStake training bag
#pragma once

#include "engine/assets/SceneAsset.hpp"
#include "engine/math/Vec2.hpp"

#include <cstdint>

namespace soulknight::game::hub {

// Native subset of Room/objects/stake EnemyAIStake:
// - 40 max HP
// - every accepted hit restores hp = max_hp (the bag never dies)
// - Animator trigger "hit" plays stake_hit_l (0.375 s)
// - face the hit source by X comparison
// - 30% of hits raise StakeHurtEvent (the native trainer line hook)
class TrainingStake {
public:
    bool Configure(const engine::assets::SceneAsset& scene);
    void Reset();

    bool Configured() const { return configured_; }
    engine::Vec2 Position() const { return position_; }
    int32_t BodySprite() const { return body_sprite_; }
    int32_t CurrentSprite() const;
    bool FacingLeft() const { return facing_left_; }
    bool HitFlash() const { return flash_timer_ > 0.0f; }
    engine::Vec2 RenderOffset() const;
    float HitTimer() const { return hit_timer_; }

    // Returns true when the hit accepted (bag never dies). `source_x` mirrors
    // the recovered single-player X comparison; `raised_event` mirrors the
    // recovered Random.Range(0,10) > 2 rejection (30% event chance).
    bool Hit(int damage, float source_x, bool* raised_event = nullptr);

    void Update(float delta_seconds);

private:
    bool configured_ = false;
    engine::Vec2 position_{0.0f, 0.0f};
    int32_t max_hp_ = 40;
    int32_t hp_ = 40;
    int32_t body_sprite_ = -1;
    std::vector<engine::assets::PetFrame> frames_;
    float hit_timer_ = 0.0f;
    float flash_timer_ = 0.0f;
    float animation_time_ = 0.0f;
    bool facing_left_ = false;
    std::uint32_t hit_serial_ = 0x9E3779B9u;
};

} // namespace soulknight::game::hub
