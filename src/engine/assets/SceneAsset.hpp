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
    // Recovered RGBox obstacle prefabs (box01..box07) carry serialized hp.
    bool breakable = false;
    int32_t hp = 0;
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
    std::string dead_sprite_id;
    int32_t dead_sprite_index = -1;
    std::string idle_animation_id;
    std::string run_animation_id;
    std::string hit_animation_id;
    std::string death_animation_id;
    std::string starting_weapon;
    int32_t max_hp = 0;
    int32_t max_armor = 0;
    int32_t max_energy = 0;
    float speed = 0.0f;
    float speed_rate = 0.0f;
};

// Recovered RoleAttributePlayer values for the playable dev-room character.
// Character switching is a dev-only asset swap and keeps this combat state.
struct PlayerStats {
    std::string character_id;
    int32_t max_hp = 0;
    int32_t hp = 0;
    int32_t max_armor = 0;
    int32_t armor = 0;
    int32_t max_energy = 0;
    int32_t energy = 0;
    int32_t camp = 1;
    int32_t atk = 0;
    int32_t critical = 0;
    float speed = 0.0f;
    float speed_rate = 0.0f;
    int32_t deviation = 0;
};

struct ProjectileDefinition {
    std::string id;
    int32_t sprite_index = -1;
    float prefab_speed = 0.0f;
    float lifetime = 5.0f;
    int32_t camp = 1;
    bool can_rebound = false;
    Vec2 collider_half_extents{0.25f, 0.20f};
    Vec2 collider_offset{0.25f, 0.0f};
    int32_t sorting_layer = 6;
    int32_t sorting_order = 0;
};


// Recovered MapManagerLevel surface theme. One theme is selected by campaign
// level (for example 1-1 => map_A1) and drives the floor/wall variants that
// RGRoomX generates at runtime.
struct LevelSurfaceSprite {
    int32_t sprite_index = -1;
    Vec2 offset{0.0f, 0.0f};
    Vec2 scale{1.0f, 1.0f};
    float rotation = 0.0f;
    Color color{255, 255, 255, 255};
    int32_t sorting_layer = 0;
    int32_t sorting_order = 0;
    bool flip_x = false;
    bool flip_y = false;
};

struct LevelFloorVariant {
    LevelSurfaceSprite sprite;
};

struct LevelWallVariant {
    std::vector<LevelSurfaceSprite> sprites;
};

struct LevelTheme {
    std::string level;
    std::string branch;
    std::string manager;
    bool use_new_floor_pattern = false;
    std::vector<LevelFloorVariant> floors;
    std::vector<LevelWallVariant> walls;
    // obstacle_list entries, indexed by RGObjectSkin.obj_index.
    std::vector<LevelWallVariant> obstacles;
};

struct WeaponVisualKey {
    float time = 0.0f;
    float value = 0.0f;
};

// One recovered bullet entry in a multi-projectile weapon (for example the
// six elemental prefabs cycled by GunMultiBullet02 / Rainbow).
struct WeaponProjectileVariant {
    ProjectileDefinition projectile;
    float bullet_speed = 0.0f;
    float size = 1.0f;
    int damage = 0;
    int critic = 0;
    int repel = 0;
    int through = 0;
};

// Serialized subclass data that is not part of the common RGWeapon fire path.
// The recovery document (docs/development/UNIQUE_WEAPON_RECOVERY.md) records
// the exact class/address for each kind; values below are serialized fields,
// never invented.
struct WeaponSpecial {
    std::string kind;
    int count = 0;
    float duration = 0.0f;
    float interval = 0.0f;
    float value = 0.0f;
    float range = 0.0f;
    int damage = 0;
    int hp = 0;
    float speed = 0.0f;
    float hold_speed = 1.0f;
    float a_size_max = 0.0f;
    float a_interval = 0.0f;
    float inactive_time = 0.0f;
    int force = 0;
    int reflect_count = 0;
    int multi_count = 1;
    int continuous_count = 1;
    int angle = 0;
    int energy = 0;
    int sprite_index = -1;
    int sprite2_index = -1;
    std::string sprite_id;
    std::string sprite2_id;
    std::string audio_clip;
    std::string audio_clip2;
    std::string audio_clip3;
    std::vector<WeaponProjectileVariant> variants;
};

struct WeaponDefinition {
    std::string id;
    std::string display_name;
    std::string class_name;
    // Recovered RGWeapon.audio_clip name (for example fx_gun_2).
    std::string audio_clip;
    std::string weapon_type_name;
    int32_t rarity = -1;
    int32_t drop_level = -1;
    bool forgeable = false;
    int32_t item_value = 0;
    std::vector<WeaponVisualKey> visual_position_x;
    std::vector<WeaponVisualKey> visual_rotation_z;
    std::vector<WeaponVisualKey> muzzle_enabled;
    float visual_attack_time = 0.0f;
    ProjectileDefinition projectile;
    float bullet_speed = 0.0f;
    float size = 1.0f;
    int32_t damage = 0;
    int32_t critic = 0;
    int32_t deviation = 0;
    int32_t bullet_count = 1;
    int32_t spread_degrees = 0;
    bool is_melee = false;
    float melee_range = 1.8f;
    float melee_arc_degrees = 110.0f;
    // RoleAttribute.deviation for the owning character. c00 does not serialize
    // this field and RoleAttribute's constructor leaves it at 0.
    int32_t owner_deviation = 0;
    // Serialized RGWeapon/Gun001 `consume`; IL2CPP symbols
    // RGWeapon.MakeConsume / MultiHand.CheckEnergyEnough identify it as the
    // per-attack energy cost. -1 means derive from the serialized field.
    int32_t consume = 0;
    int32_t energy_cost = -1;
    int EnergyCost() const { return energy_cost >= 0 ? energy_cost : consume; }
    float weapon_speed = 1.0f;
    // Native display scale. CONFIRMED from the serialized weapon_000,
    // weapon_002, weapon_048 and weapon_287 prefabs: the root Transform and
    // child `w` Transform both have localScale (1, 1, 1). The earlier 1.5x
    // presentation experiment is retired; muzzle/projectile math is unchanged.
    float visual_scale = 1.0f;
    // RGWeapon.atk_move_speed; AttackStart adds this to speed_rate and
    // AttackStop subtracts it. weapon_000 serializes -0.1.
    float atk_move_speed = 0.0f;
    int32_t repel = 0;
    int32_t through = 0;
    // Recovered from weapon_000's Animator: the `w_pistol` state loops for
    // 0.333333 s and carries an `Attack` animation event at 0.033333 s.
    // `RGWeapon.Start` scales Animator.speed by weapon_speed; weapon_000 uses
    // 1.0, so the effective period is 1/3 s with a 1/30 s first-shot delay.
    float attack_cycle_seconds = 1.0f / 3.0f;
    float first_shot_delay_seconds = 1.0f / 30.0f;
    // Scaled animation-event times within one attack cycle. Empty means the
    // generic one-shot-per-cycle path. Non-empty drives multi-event bursts
    // (Assault Sniper Rifle, Gun008 revolver family) exactly as recovered.
    std::vector<float> attack_event_times;
    // Recovered subclass fields and alternate projectile prefabs.
    WeaponSpecial special;

    // Recovered weapon prefab visual (`w` child SpriteRenderer) and gun_point
    // local offset. The devroom renders the real weapon texture in the hand.
    int32_t visual_sprite_index = -1;
    Vec2 visual_offset{0.0f, 0.0f};
    Vec2 muzzle_offset{0.6f, 0.15f};
    std::string muzzle_sprite;
    float visual_rotation = 0.0f;
    int32_t sorting_layer = 5;
    int32_t sorting_order = 1;
};

struct EnemyDefinition {
    std::string id;
    int32_t body_sprite_index = -1;
    int32_t dead_sprite_index = -1;
    int32_t shadow_sprite_index = -1;
    int32_t hp = 0;
    int32_t speed = 0;
    int32_t camp = 0;
    int32_t contact_damage = 0;
    float contact_cooldown = 0.0f;
    CollisionBox collider{};
    // RGEController ctor default dead_destory_delay_time = 4.0 s. The 0.95
    // float written on hurt is `friction`, not a death delay.
    float death_despawn_delay = 4.0f;
    bool ranged = false;
    ProjectileDefinition projectile;
    float projectile_speed = 0.0f;
    int32_t projectile_damage = 0;
    float shoot_cooldown = 2.0f;
    float attack_range = 8.0f;
    std::string class_name;
    bool stand_in_shooting = false;
    bool lock_in_shooting = false;
    float shoot_time = 0.0f;
    int32_t projectile_count = 1;
    float fan_angle = 0.0f;
    int32_t burst_count = 1;
    float burst_interval = 0.0f;
    float dash_speed = 0.0f;
    float dash_duration = 0.0f;
    float death_explosion_radius = 0.0f;
    int32_t death_explosion_damage = 0;
    bool stationary = false;
    bool mimic = false;
    bool invulnerable_while_attacking = false;
    int32_t body_sorting_layer = 5;
    int32_t body_sorting_order = 1;
    int32_t shadow_sorting_layer = 3;
    int32_t shadow_sorting_order = 0;
};

struct EnemyFixture {
    std::string definition;
    Vec2 position{0.0f, 0.0f};
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

struct PetFrame {
    float time = 0.0f;
    int32_t sprite_index = -1;
};

// Recovered RGPetController family definition. Follow distances, cooldown and
// damage are serialized prefab values; sprites come from the pet Animator.
struct PetDefinition {
    std::string id;
    std::string display_name;
    std::string class_name;
    int32_t max_hp = 0;
    int32_t damage = 0;
    float speed = 0.0f;
    float speed_rate = 0.0f;
    float atk_cd = 2.0f;
    float min_follow_distance = 2.0f;
    float max_follow_distance = 20.0f;
    float scout_distance = 8.0f;
    float atk_range = 2.0f;
    bool is_melee = true;
    int32_t body_sprite_index = -1;
    int32_t shadow_sprite_index = -1;
    std::vector<PetFrame> idle_frames;
    std::vector<PetFrame> run_frames;
    std::vector<PetFrame> attack_frames;
    std::vector<PetFrame> dead_frames;
};

struct InteractionMarker {
    std::string id;
    std::string kind;
    std::string label;
    Vec2 position{0.0f, 0.0f};
    float radius = 2.0f;
    // TalkCharNpc/TalkTrigger payload. Empty for non-NPC markers.
    std::string speaker;
    std::string text;
    std::string text_alt;
    std::string dialogue_term;
    std::string character_id;
    std::string pet_id;
    int seller_index = -1;
    int price = 0;
    std::string item_id;
};

// Recovered EnemyAIStake (room/objects/stake): 40 HP, stake_hit_l 0.375 s,
// HP is restored to max on every accepted hit and the object never dies.
struct TrainingStake {
    std::string id;
    Vec2 position{0.0f, 0.0f};
    int32_t hp = 40;
    float hit_duration = 0.375f;
    int32_t body_sprite_index = -1;
    std::vector<PetFrame> frames;
};

struct RoomMetadata {
    int type = 0;
    std::string type_name = "normal";
    int stage = 0;
    int number = 0;
    std::string label;
    std::string clear_label = "clear";
};

struct SceneAsset {
    std::string id;
    RoomMetadata room;
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
    PlayerStats player_stats;
    std::vector<WeaponDefinition> weapons;
    std::vector<LevelTheme> level_themes;
    std::vector<Vec2> floor_cells;
    std::vector<Vec2> aisle_floor_cells;
    std::vector<Vec2> wall_cells;
    std::vector<Vec2> corridor_wall_cells;
    std::vector<EnemyDefinition> enemy_definitions;
    std::vector<EnemyFixture> enemy_fixtures;
    std::vector<SceneDoor> doors;
    std::vector<InteractionMarker> interaction_markers;
    std::vector<PetDefinition> pet_definitions;
    bool has_training_stake = false;
    TrainingStake training_stake;
    bool has_room_topology = false;
    bool is_hub = false;

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

    int32_t FindEnemyDefinitionIndex(const std::string& definition_id) const {
        for (std::size_t i = 0; i < enemy_definitions.size(); ++i) {
            if (enemy_definitions[i].id == definition_id) {
                return static_cast<int32_t>(i);
            }
        }
        return -1;
    }
};

} // namespace soulknight::engine::assets
