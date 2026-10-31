// Soul Knight Linux — native scene data
#pragma once

#include "DoorAnimation.hpp"
#include "SpriteData.hpp"
#include "TextureData.hpp"
#include "engine/animation/AnimationClip.hpp"
#include "engine/math/Color.hpp"
#include "engine/math/Vec2.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace soulknight::engine::assets {

struct TextureAsset {
    uint32_t id = 0;
    std::string name;
    std::string png_path;
    TextureData data;
    TextureFilter filter = TextureFilter::Point;
    TextureWrap wrap = TextureWrap::Clamp;
};

struct SpriteInstance {
    uint32_t sprite_index = 0;
    Vec2 position{0.0f, 0.0f};
    float rotation = 0.0f; // radians, Y-up
    Vec2 scale{1.0f, 1.0f};
    Color color = Color::White();
    bool flip_x = false;
    bool flip_y = false;
    int32_t sorting_layer = 0;
    int32_t sorting_order = 0;
    std::string path; // source hierarchy path, evidence only
    std::string door; // optional recovered door side this leaf belongs to
    std::string door_role; // leaf / shadow / ornament
    std::string door_leaf; // d1..d5 for door-only instances
};

struct CollisionBox {
    Vec2 min{0.0f, 0.0f};
    Vec2 max{0.0f, 0.0f};

    bool IsValid() const {
        return max.x > min.x && max.y > min.y;
    }
};

enum class DoorOneWayBlock {
    None,
    PositiveX,
    NegativeX,
    PositiveY,
    NegativeY,
};

struct DoorLeafBinding {
    std::string name; // d1..d5
    int32_t leaf_instance = -1;
    int32_t shadow_instance = -1;
    int32_t ornament_instance = -1;
};

struct ObstacleVisual {
    int32_t sprite_index = -1;
    Vec2 offset{0.0f, 0.0f};
    float rotation = 0.0f;
    Vec2 scale{1.0f, 1.0f};
    Color color = Color::White();
    int32_t sorting_layer = 0;
    int32_t sorting_order = 0;
    bool flip_x = false;
    bool flip_y = false;
};

struct ObstacleVariant {
    int32_t index = 0;
    std::string name;
    std::string path_id;
    CollisionBox collider{};
    bool has_collider = false;
    bool collider_trigger = false;
    std::vector<ObstacleVisual> visuals;
};

struct ObstacleMarker {
    Vec2 position{0.0f, 0.0f};
    int32_t variant = 0;
    std::string name;
    std::string path;
};

struct SceneCharacter {
    std::string id;
    std::string controller_name;
    std::string controller_path_id;
    std::string source_path_id;
    std::string body_sprite_id;
    int32_t body_sprite_index = -1;
    std::string idle_animation_id;
    std::string run_animation_id;
};

struct SceneDoor {
    std::string id;      // north/south/east/west
    std::string aisle;   // aisle_n/aisle_s/aisle_e/aisle_w
    std::string door_path_id;
    std::string trigger_path_id;
    int32_t direction = 0;            // RGAisle direction: east=0, south=1, west=2, north=3
    int32_t enter_exit_direction = 0; // emRoomDoorDir: north=1, south=2, west=3, east=4
    CollisionBox barrier{};
    CollisionBox trigger{};
    DoorOneWayBlock one_way_block = DoorOneWayBlock::None;
    bool enabled = false;
    bool open = false;

    // Recovered Animator clips and scene instance bindings. The door root and
    // leaf roots stay static; animation only changes the leaf sprite, ornament
    // local y, renderer visibility and collision timing.
    DoorAnimationClip open_animation;
    DoorAnimationClip close_animation;
    std::vector<DoorLeafBinding> leaves;

    bool HasAnimation() const {
        return open_animation.IsValid() || close_animation.IsValid();
    }
};

struct SceneCamera {
    Vec2 position{0.0f, 0.0f};
    float orthographic_size = 10.0f;
    float viewport_width = 1920.0f;
    float viewport_height = 1080.0f;
};

struct SceneAsset {
    std::string id;
    std::vector<TextureAsset> textures;
    std::vector<SpriteData> sprites;
    std::vector<SpriteInstance> instances;
    std::vector<engine::animation::AnimationClip> animations;
    SceneCamera camera;
    Vec2 spawn{0.0f, 0.0f};
    Rect collision_bounds{0.0f, 0.0f, 0.0f, 0.0f};
    Rect interior_bounds{0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<CollisionBox> collision_solids;
    std::vector<ObstacleVariant> obstacle_variants;
    std::vector<ObstacleMarker> obstacle_markers;
    std::vector<SceneCharacter> characters;
    std::vector<SceneDoor> doors;
    bool has_room_topology = false;

    int32_t FindSpriteIndex(const std::string& sprite_id) const {
        for (std::size_t i = 0; i < sprites.size(); ++i) {
            if (sprites[i].id == sprite_id) return static_cast<int32_t>(i);
        }
        return -1;
    }

    int32_t FindDoorIndex(const std::string& door_id) const {
        for (std::size_t i = 0; i < doors.size(); ++i) {
            if (doors[i].id == door_id) return static_cast<int32_t>(i);
        }
        return -1;
    }

    int32_t FindAnimationIndex(const std::string& animation_id) const {
        for (std::size_t i = 0; i < animations.size(); ++i) {
            if (animations[i].id == animation_id) {
                return static_cast<int32_t>(i);
            }
        }
        return -1;
    }
};

} // namespace soulknight::engine::assets
