// Soul Knight Linux — explicit player scene-to-render binding
#include "PlayerRenderSync.hpp"

namespace soulknight::engine::rendering {

PlayerRenderBinding BindPlayerRenderInstances(
    const assets::SceneAsset& scene,
    std::string_view body_path,
    std::string_view shadow_path) {
    PlayerRenderBinding binding;
    for (std::size_t i = 0; i < scene.instances.size(); ++i) {
        const assets::SpriteInstance& instance = scene.instances[i];
        if (instance.path == body_path) {
            binding.body_instance = static_cast<int32_t>(i);
        } else if (instance.path == shadow_path) {
            binding.shadow_instance = static_cast<int32_t>(i);
        }
    }
    if (!binding.IsValid()) return binding;

    const Vec2& body = scene.instances[static_cast<std::size_t>(
        binding.body_instance)].position;
    const Vec2& shadow = scene.instances[static_cast<std::size_t>(
        binding.shadow_instance)].position;
    // Recovered Unity local offset: body (20,20), shadow (20,19.95).
    binding.shadow_offset = shadow - body;
    return binding;
}

Vec2 ResolvePlayerRenderPosition(
    const assets::SceneAsset& scene,
    const PlayerRenderBinding& binding,
    int32_t instance_index,
    const Vec2& player_world_position) {
    if (instance_index == binding.body_instance) {
        return player_world_position;
    }
    if (instance_index == binding.shadow_instance) {
        return player_world_position + binding.shadow_offset;
    }
    if (instance_index >= 0 &&
        static_cast<std::size_t>(instance_index) < scene.instances.size()) {
        return scene.instances[static_cast<std::size_t>(instance_index)].position;
    }
    return {};
}

} // namespace soulknight::engine::rendering
