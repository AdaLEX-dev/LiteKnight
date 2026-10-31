// Soul Knight Linux — explicit player scene-to-render binding
#pragma once

#include "engine/assets/SceneAsset.hpp"
#include "engine/math/Vec2.hpp"

#include <cstdint>
#include <string_view>

namespace soulknight::engine::rendering {

// The player visual is part of the recovered scene hierarchy, not a separate
// entity. This binding is the only place that maps the simulated player
// transform onto c00/img/body and c00/shadow.
struct PlayerRenderBinding {
    int32_t body_instance = -1;
    int32_t shadow_instance = -1;
    Vec2 shadow_offset{0.0f, 0.0f};

    bool IsValid() const {
        return body_instance >= 0 && shadow_instance >= 0;
    }
};

// Finds the recovered body/shadow instances by hierarchy path. Returns an
// invalid binding when the scene does not contain the player visuals.
PlayerRenderBinding BindPlayerRenderInstances(
    const assets::SceneAsset& scene,
    std::string_view body_path = "c00/img/body",
    std::string_view shadow_path = "c00/shadow");

// Resolves the rendered world position for a scene instance. The body follows
// the simulated player exactly; the shadow keeps its recovered relative offset.
// Every other instance keeps its authored scene position. This function never
// reads the camera: camera follow is an independent presentation concern.
Vec2 ResolvePlayerRenderPosition(
    const assets::SceneAsset& scene,
    const PlayerRenderBinding& binding,
    int32_t instance_index,
    const Vec2& player_world_position);

} // namespace soulknight::engine::rendering
