// Soul Knight Linux — recovered character assets for dev-room switching
#include "PlayerCharacterSet.hpp"

namespace soulknight::game::player {

bool PlayerCharacterSet::Configure(const engine::assets::SceneAsset& scene,
                                   std::string* error) {
    characters_ = scene.characters;
    idle_clips_.clear();
    run_clips_.clear();
    death_clips_.clear();
    active_ = 0;
    configured_ = false;
    if (error != nullptr) error->clear();

    if (characters_.empty()) {
        if (error != nullptr) *error = "scene has no recovered characters";
        return false;
    }
    for (const engine::assets::SceneCharacter& character : characters_) {
        const int32_t idle = scene.FindAnimationIndex(character.idle_animation_id);
        const int32_t run = scene.FindAnimationIndex(character.run_animation_id);
        if (idle < 0 || run < 0) {
            if (error != nullptr) {
                *error = "character animation is missing: " + character.id;
            }
            return false;
        }
        if (character.body_sprite_index < 0 ||
            static_cast<std::size_t>(character.body_sprite_index) >=
                scene.sprites.size()) {
            if (error != nullptr) {
                *error = "character body sprite is missing: " + character.id;
            }
            return false;
        }
        idle_clips_.push_back(&scene.animations[static_cast<std::size_t>(idle)]);
        run_clips_.push_back(&scene.animations[static_cast<std::size_t>(run)]);
        const int32_t death =
            scene.FindAnimationIndex(character.death_animation_id);
        death_clips_.push_back(death >= 0
            ? &scene.animations[static_cast<std::size_t>(death)] : nullptr);
    }
    configured_ = true;
    return true;
}

bool PlayerCharacterSet::Select(std::size_t index) {
    if (index >= characters_.size()) return false;
    active_ = index;
    return true;
}

void PlayerCharacterSet::Cycle(int delta) {
    if (characters_.empty()) return;
    const int count = static_cast<int>(characters_.size());
    int next = (static_cast<int>(active_) + delta) % count;
    if (next < 0) next += count;
    active_ = static_cast<std::size_t>(next);
}

} // namespace soulknight::game::player
