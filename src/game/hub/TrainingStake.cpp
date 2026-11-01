#include "TrainingStake.hpp"

#include <algorithm>
#include <cmath>

namespace soulknight::game::hub {

bool TrainingStake::Configure(const engine::assets::SceneAsset& scene) {
    configured_ = false;
    if (!scene.has_training_stake) return false;
    const engine::assets::TrainingStake& stake = scene.training_stake;
    position_ = stake.position;
    max_hp_ = std::max(1, stake.hp);
    hp_ = max_hp_;
    body_sprite_ = stake.body_sprite_index;
    frames_ = stake.frames;
    hit_timer_ = 0.0f;
    flash_timer_ = 0.0f;
    animation_time_ = 0.0f;
    facing_left_ = false;
    configured_ = true;
    return true;
}

void TrainingStake::Reset() {
    hp_ = max_hp_;
    hit_timer_ = 0.0f;
    flash_timer_ = 0.0f;
    animation_time_ = 0.0f;
}

bool TrainingStake::Hit(int damage, float source_x, bool* raised_event) {
    if (!configured_) return false;
    (void)damage;
    hp_ = max_hp_;
    hit_timer_ = 0.375f;
    animation_time_ = 0.0f;
    flash_timer_ = 0.10f;
    facing_left_ = source_x > position_.x;
    // Unity Random.Range(0, 10) > 2 returns early; the event fires for 0..2.
    hit_serial_ = hit_serial_ * 1664525u + 1013904223u;
    const bool raised = (hit_serial_ % 10u) <= 2u;
    if (raised_event != nullptr) *raised_event = raised;
    return true;
}

void TrainingStake::Update(float delta_seconds) {
    if (delta_seconds <= 0.0f) return;
    hit_timer_ = std::max(0.0f, hit_timer_ - delta_seconds);
    flash_timer_ = std::max(0.0f, flash_timer_ - delta_seconds);
    if (hit_timer_ > 0.0f) animation_time_ += delta_seconds;
}

int32_t TrainingStake::CurrentSprite() const {
    if (frames_.empty()) return body_sprite_;
    if (hit_timer_ <= 0.0f) return body_sprite_;
    const float elapsed = std::min(animation_time_,
                                   std::max(0.0f, frames_.back().time));
    int32_t sprite = frames_.front().sprite_index;
    for (const engine::assets::PetFrame& frame : frames_) {
        if (frame.time > elapsed) break;
        sprite = frame.sprite_index;
    }
    return sprite;
}

engine::Vec2 TrainingStake::RenderOffset() const {
    if (hit_timer_ <= 0.0f) return {0.0f, 0.0f};
    const float phase = 0.375f - hit_timer_;
    return {std::sin(phase * 90.0f) * 0.05f, 0.0f};
}

} // namespace soulknight::game::hub
