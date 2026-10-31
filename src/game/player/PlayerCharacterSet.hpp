// Soul Knight Linux — recovered character assets for dev-room switching
#pragma once

#include "engine/assets/SceneAsset.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace soulknight::game::player {

// Developer/test character set. This is not the production character-select
// UI: it only exposes the recovered c00..c14 character prefabs and their
// serialized idle/run clips to the existing player locomotion/render path.
class PlayerCharacterSet {
public:
    PlayerCharacterSet() = default;

    bool Configure(const engine::assets::SceneAsset& scene, std::string* error);
    bool Configured() const { return configured_; }

    std::size_t Count() const { return characters_.size(); }
    std::size_t ActiveIndex() const { return active_; }
    const engine::assets::SceneCharacter& Active() const {
        return characters_[active_];
    }

    bool Select(std::size_t index);
    void Cycle(int delta);

    const engine::animation::AnimationClip* IdleClip() const {
        return idle_clips_[active_];
    }
    const engine::animation::AnimationClip* RunClip() const {
        return run_clips_[active_];
    }

private:
    std::vector<engine::assets::SceneCharacter> characters_;
    std::vector<const engine::animation::AnimationClip*> idle_clips_;
    std::vector<const engine::animation::AnimationClip*> run_clips_;
    std::size_t active_ = 0;
    bool configured_ = false;
};

} // namespace soulknight::game::player
