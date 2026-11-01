// Soul Knight Linux — recovered MapManagerLevel surface generation
#pragma once

#include "engine/assets/SceneAsset.hpp"
#include "engine/math/Vec2.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace soulknight::game::room {

struct SurfaceInstance {
    int32_t sprite_index = -1;
    engine::Vec2 position{0.0f, 0.0f};
    engine::Vec2 scale{1.0f, 1.0f};
    float rotation = 0.0f;
    engine::Color color{255, 255, 255, 255};
    int32_t sorting_layer = 0;
    int32_t sorting_order = 0;
    bool flip_x = false;
    bool flip_y = false;
};

// Native implementation of RGRoomX.CreateFloor/CreateWall plus
// RGAisle.CreateFloor/CreateAisleWall using the recovered MapManagerLevel
// floor_list/wall_list. Geometry comes from the PatternRoom topology; only
// surface sprites change with the campaign level.
class LevelSurfaceBuilder {
public:
    static const engine::assets::LevelTheme* Find(
        const engine::assets::SceneAsset& scene, const std::string& level,
        const std::string& branch = "A");

    // Deterministic native policy: the reference seeds room RNG from time and
    // network state. Replays need a stable stream, so the caller supplies a
    // room seed derived from map seed + room/template identity.
    static std::vector<SurfaceInstance> Build(
        const engine::assets::SceneAsset& scene,
        const engine::assets::LevelTheme& theme, std::uint32_t room_seed);
};

} // namespace soulknight::game::room
