#include <filesystem>
// Soul Knight Linux — Application lifecycle implementation
#include "Application.hpp"
#include "AssetEnvironment.hpp"

#include "engine/assets/PngDecoder.hpp"
#include "engine/assets/SceneLoader.hpp"
#include "engine/animation/AnimationClip.hpp"
#include "engine/core/Log.hpp"
#include "engine/rendering/VulkanRenderer.hpp"
#include "engine/rendering/WorldDepth.hpp"
#include "game/weapons/WeaponRenderer.hpp"
#include "game/ui/AssetLab.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <functional>
#include <chrono>
#include <random>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>
#include <fstream>

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

namespace soulknight::app {

using soulknight::game::room::BuildDungeonPlan;
using soulknight::game::room::DungeonPlan;
using soulknight::game::room::DungeonRoomTemplate;
using soulknight::game::room::Room;
using soulknight::game::room::RoomConnection;

namespace {

double NowSeconds() {
    using namespace std::chrono;
    const auto now = steady_clock::now();
    return duration<double>(now.time_since_epoch()).count();
}

const char* BooleanText(bool value) {
    return value ? "true" : "false";
}

engine::rendering::SortLayer MapSortingLayer(int32_t unity_sorting_layer) {
    switch (unity_sorting_layer) {
        case 1: return engine::rendering::SortLayer::BackGround;
        case 2: return engine::rendering::SortLayer::Floor;
        case 3: return engine::rendering::SortLayer::Shadow;
        case 4: return engine::rendering::SortLayer::Wall;
        case 5: return engine::rendering::SortLayer::Character;
        case 6: return engine::rendering::SortLayer::WallFont;
        case 7: return engine::rendering::SortLayer::Effect;
        case 8: return engine::rendering::SortLayer::UI;
        default: return engine::rendering::SortLayer::BackGround;
    }
}

namespace {
struct MechOffer {
    const char* id;
    const char* name;
    int cost;
    int defence;
    float speed_rate;
    const char* unlock_key;
};
// CONFIRMED MonoBehaviour fields on the recovered m_mech_0..m_mech_5 mount
// prefabs (itemValue/defence/speedRate/unlock_key). Names come from the exact
// m_mech_* localization terms. The authoritative MechInfos material table is
// not present in the extracted Resources data, so locked blueprints remain
// locked and no material cost is invented.
constexpr MechOffer kMechOffers[] = {
    {"m_mech_0", "Prototype Armor", 12, 1, -0.20f, ""},
    {"m_mech_1", "Iron Fist", 12, 1, -0.20f, ""},
    {"m_mech_2", "Apocalypse", 20, 1, -0.20f, "blueprint_m_mech_2"},
    {"m_mech_3", "C5H6O5", 40, 2, -0.20f, "blueprint_m_mech_3"},
    {"m_mech_4", "WiFi Booster", 40, 0, 0.00f, "blueprint_m_mech_4"},
    {"m_mech_5", "Dragon Dance Armor", 20, 1, -0.20f,
     "blueprint_m_mech_5"},
};
} // namespace

constexpr const char* kSuggestionTypes[] = {
    "wall", "remove-wall", "collision", "clipping", "depth",
    "missing-prop", "wrong-prop", "floor-boundary", "garden-boundary",
    "dark", "note"};

double UnixSeconds() {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

engine::collision::Aabb MakeFallbackCollisionBounds(const engine::Vec2& center) {
    // NATIVE DESIGN: compatibility fallback for scenes without explicit
    // collision solids. The r1_1 scene always supplies recovered topology.
    constexpr float kInteriorHalfExtent = 6.5f;
    return engine::collision::Aabb{
        {center.x - kInteriorHalfExtent, center.y - kInteriorHalfExtent},
        {center.x + kInteriorHalfExtent, center.y + kInteriorHalfExtent}
    };
}

engine::collision::Aabb ToAabb(const engine::assets::CollisionBox& box) {
    return engine::collision::Aabb{box.min, box.max};
}

} // namespace

Application::Application(AppConfig config) : config_(std::move(config)) {}

Application::~Application() {
    Shutdown();
}

void Application::AssignUniqueTextureIds(engine::assets::SceneAsset& scene,
                                         uint32_t base) {
    // SceneLoader assigns texture ids from zero in every scene. Rooms share one
    // renderer texture bank, so ids must be unique before uploads; otherwise a
    // transition renders the new room with the previous room's atlas.
    for (std::size_t i = 0; i < scene.textures.size(); ++i) {
        scene.textures[i].id = base + static_cast<uint32_t>(i);
    }
    for (engine::assets::SpriteData& sprite : scene.sprites) {
        if (sprite.texture_id < scene.textures.size()) {
            sprite.texture_id = scene.textures[sprite.texture_id].id;
        }
    }
}

bool Application::Initialize() {
    if (initialized_) return true;

    LOG_INFO_APP("LiteKnight native runtime");
    LOG_INFO_APP("Platform: {}", SDL_GetPlatform());
    LOG_INFO_APP("Version: {} ({})", LITEKNIGHT_VERSION, SOUL_KNIGHT_BUILD_TYPE);
    LOG_INFO_APP("Commit: {}", SOUL_KNIGHT_COMMIT_HASH);
    if (config_.devroom) {
        LOG_INFO_APP("Mode: developer devroom");
    }
    LOG_INFO_APP("Target: Wayland only, Vulkan 1.3, 1920x1080");
    LOG_INFO_APP("Timing: {} Hz simulation, {} FPS render target",
                 kSimulationHz, kRenderTargetHz);

    // Wayland-only contract. The hint is set before SDL video initialization so
    // SDL cannot choose X11/XWayland as a fallback. The post-init check below is
    // the actual contract enforcement.
    (void)SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    (void)SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR, "0");
    (void)SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY, "1");
    (void)SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    (void)SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        LOG_FATAL_APP("SDL_Init failed: {}", SDL_GetError());
        return false;
    }
    sdl_initialized_ = true;

    const char* video_driver = SDL_GetCurrentVideoDriver();
    if (!video_driver || !IsWaylandDriver(std::string_view(video_driver))) {
        LOG_FATAL_APP("Wayland-only mode violated: active SDL driver is '{}'",
                      video_driver ? video_driver : "<none>");
        LOG_FATAL_APP("Refusing X11, XWayland, KMSDRM, or any non-Wayland backend");
        return false;
    }
    LOG_INFO_APP("SDL video driver: {}", video_driver);
    LOG_INFO_APP("Wayland-only mode: enabled");

    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        LOG_FATAL_APP("SDL_Vulkan_LoadLibrary failed: {}", SDL_GetError());
        return false;
    }
    vulkan_library_loaded_ = true;
    LOG_INFO_APP("SDL revision: {}", SDL_GetRevision());

    if (!filesystem_.Initialize()) {
        LOG_FATAL_APP("Filesystem initialization failed");
        return false;
    }
    const AssetEnvironmentReport asset_environment =
        DetectAssetEnvironment(filesystem_);
    project_root_ = asset_environment.project_root;
    scene_sha256_ = asset_environment.scene_sha256;
    reference_apk_sha256_ = asset_environment.reference_apk_sha256;
    asset_inventory_sha256_ = asset_environment.asset_inventory_sha256;
    LogAssetEnvironment(asset_environment);
    platform::AudioEngine::Config audio_config;
    if (audio_engine_.Initialize(audio_config)) {
        audio_available_ = true;
        LOG_INFO_APP("Reference audio engine initialized: {} Hz, {} ch",
                     audio_config.sample_rate, audio_config.channels);
    } else {
        LOG_WARN_APP("Audio unavailable; continuing without sound");
    }
    save_manager_ = std::make_unique<soulknight::game::save::SaveManager>(
        filesystem_.GetDataHome() / "liteknight" / "saves");
    LOG_INFO_APP("Native save slot: {}",
                 save_manager_->SlotPath().string());
    RefreshSaveSummary();
    LoadUiLayout();
    std::string ui_asset_error;
    if (ui_assets_.Load(filesystem_, "ui/manifest.json", &ui_asset_error)) {
        LOG_INFO_APP("Recovered UI atlases: {} textures",
                     ui_assets_.Textures().size());
    } else {
        LOG_WARN_APP("Recovered UI atlases unavailable ({}); using native panels",
                     ui_asset_error);
    }
    if (config_.replay && !LoadReplayIfRequested()) {
        return false;
    }

    texture_uploads_.clear();
    camera_ = engine::Camera2D{};
    locomotion_animator_ = {};
    player_ = {};
    collision_world_ = {};
    player_render_ = {};
    room_doors_ = {};
    obstacles_ = {};
    player_characters_ = {};
    weapons_ = {};
    weapon_inventory_ = {};
    weapon_pickups_ = {};
    plant_system_ = {};
    dev_ui_ = {};
    asset_lab_ = {};
    weapon_ = {};
    projectiles_.Clear();
    stored_rooms_.clear();
    item_world_.Reset();
    ++texture_generation_;
    room_enemy_snapshots_.clear();
    cleared_rooms_.clear();
    chest_spawned_rooms_.clear();
    visited_rooms_.clear();
    player_coins_ = 0;
    ui_design_mode_ = false;
    ui_design_dragging_ = false;
    ui_design_selected_.clear();
    multi_room_world_ = false;
    map_seed_ = config_.map_seed;
    enemies_ = {};
    player_health_ = {};
    player_energy_ = {};
    aim_direction_ = {1.0f, 0.0f};
    mouse_aim_valid_ = true;
    facing_ = engine::locomotion::FacingDirection::Right;
    previous_aim_direction_ = aim_direction_;
    render_aim_direction_ = aim_direction_;
    player_dead_ = false;
    attack_held_ = false;
    movement_input_ = {};
    scene_ = {};
    last_animation_sprite_.clear();
    last_animation_state_ = engine::animation::LocomotionState::Idle;
    animation_logs_remaining_ = 8;
    scene_mode_ = !config_.scene_path.empty();

    if (scene_mode_) {
        std::string scene_error;
        if (!engine::assets::SceneLoader::Load(filesystem_, config_.scene_path,
                                               scene_, &scene_error)) {
            LOG_FATAL_APP("Scene load failed: {}", scene_error);
            return false;
        }
        if (scene_.textures.empty() || scene_.sprites.empty() ||
            scene_.instances.empty()) {
            LOG_FATAL_APP("Scene loaded with no textures/sprites/instances");
            return false;
        }
        std::string biome_error;
        if (biome_.Load(filesystem_, "scenes/biomes.json", &biome_error)) {
            LOG_INFO_APP("Biomes: {} recovered levels, current={}",
                         biome_.Count(),
                         biome_.Current() != nullptr
                             ? biome_.Current()->id : "none");
        } else {
            LOG_WARN_APP("Biomes: {}", biome_error);
        }
        LoadWorldRooms();
        std::string catalogue_error;
        if (weapon_catalogue_.Load(filesystem_,
                                   "scenes/items/weapon_catalogue.json",
                                   &catalogue_error)) {
            LOG_INFO_APP("Weapon catalogue: {} recovered entries",
                         weapon_catalogue_.Count());
        } else {
            LOG_WARN_APP("Weapon catalogue unavailable ({}); scene weapons only",
                         catalogue_error);
        }
        std::string item_error;
        if (item_assets_.Load(filesystem_, "scenes/items/items.json",
                              &item_error)) {
            item_assets_.AppendToScene(scene_);
            for (StoredRoom& stored : stored_rooms_) {
                item_assets_.AppendToScene(stored.scene);
            }
            item_world_.Reset();
            LOG_INFO_APP("Item assets: {} chest(s), {} coin type(s)",
                         item_assets_.Chests().size(),
                         item_assets_.Coins().size());
        } else {
            LOG_WARN_APP("Item assets unavailable ({}); chests/coins disabled",
                         item_error);
        }
        std::string workshop_error;
        if (workshop_.Load(filesystem_, "scenes/items/weapon_materials.json",
                           &workshop_error)) {
            LOG_INFO_APP("Workshop: {} recovered recipes",
                         workshop_.Recipes().size());
        } else {
            LOG_WARN_APP("Workshop: {}", workshop_error);
        }
        AssignUniqueTextureIds(scene_, 0x01000000u);
        for (std::size_t i = 0; i < stored_rooms_.size(); ++i) {
            AssignUniqueTextureIds(stored_rooms_[i].scene,
                                   0x01010000u +
                                   static_cast<uint32_t>(i) * 0x10000u);
        }
        std::vector<uint32_t> uploaded_ids;
        for (const engine::assets::TextureAsset& texture : scene_.textures) {
            texture_uploads_.push_back(engine::rendering::TextureUpload{
                texture.id, &texture.data, texture.filter, texture.wrap
            });
            uploaded_ids.push_back(texture.id);
        }
        for (const StoredRoom& stored : stored_rooms_) {
            for (const engine::assets::TextureAsset& texture :
                 stored.scene.textures) {
                if (std::find(uploaded_ids.begin(), uploaded_ids.end(),
                              texture.id) != uploaded_ids.end()) {
                    continue;
                }
                texture_uploads_.push_back(engine::rendering::TextureUpload{
                    texture.id, &texture.data, texture.filter, texture.wrap
                });
                uploaded_ids.push_back(texture.id);
            }
        }
        for (const auto& texture : ui_assets_.Textures()) {
            texture_uploads_.push_back(engine::rendering::TextureUpload{
                texture.id, &texture.data,
                engine::assets::TextureFilter::Point,
                engine::assets::TextureWrap::Clamp
            });
        }
        std::string font_error;
        if (dev_ui_.LoadPixelFont(filesystem_, "ui/font/pixel_bold.json",
                                  &font_error)) {
            LOG_INFO_APP("Pixel font: recovered pixel_bold atlas loaded");
        } else {
            LOG_WARN_APP("Pixel font unavailable ({}); using developer fallback",
                         font_error);
        }
        if (!dev_ui_.BuildFont()) {
            LOG_FATAL_APP("Developer UI font atlas failed");
            return false;
        }
        texture_uploads_.push_back(engine::rendering::TextureUpload{
            engine::rendering::DebugTextRenderer::kTextureId,
            &dev_ui_.FontTexture(),
            engine::assets::TextureFilter::Point,
            engine::assets::TextureWrap::Clamp
        });
        camera_.position = scene_.camera.position;
        camera_.orthographic_size = scene_.camera.orthographic_size;
        camera_.viewport_width = scene_.camera.viewport_width;
        camera_.viewport_height = scene_.camera.viewport_height;
        ConfigureCameraBounds();
        if (camera_bounds_valid_) {
            camera_.ClampToRect(camera_bounds_.min, camera_bounds_.max);
        }
        LOG_INFO_APP("Scene loaded: {} ({} textures, {} sprites, {} instances, {} animations)",
                     scene_.id, scene_.textures.size(), scene_.sprites.size(),
                     scene_.instances.size(), scene_.animations.size());
        for (const engine::animation::AnimationClip& clip : scene_.animations) {
            LOG_INFO_APP("Animation: {} ({} s, {} Hz, loop={}, tracks={})",
                         clip.id, clip.duration, clip.sample_rate,
                         BooleanText(clip.loop), clip.tracks.size());
        }
        std::string character_error;
        if (!player_characters_.Configure(scene_, &character_error)) {
            LOG_FATAL_APP("Character configuration failed: {}", character_error);
            return false;
        }
        locomotion_animator_.Configure(player_characters_.IdleClip(),
                                       player_characters_.RunClip(),
                                       player_characters_.DeathClip());
        player_ = engine::locomotion::PlayerLocomotion(scene_.spawn);
        game_ui_.SetCharacterCount(
            static_cast<int>(player_characters_.Count()));
        previous_player_position_ = player_.Position();
        render_player_position_ = player_.Position();
        render_camera_ = camera_;
        visited_rooms_.clear();
        visited_rooms_.push_back(scene_.id);
        previous_camera_position_ = camera_.position;
        facing_ = engine::locomotion::FacingDirection::Right;
        player_render_ = engine::rendering::BindPlayerRenderInstances(scene_);
        if (!player_render_.IsValid()) {
            LOG_FATAL_APP("Recovered player body/shadow render instances are missing");
            return false;
        }
        LOG_INFO_APP("Player render binding: body instance {}, shadow instance {}, "
                     "shadow offset ({:.3f},{:.3f})",
                     player_render_.body_instance, player_render_.shadow_instance,
                     player_render_.shadow_offset.x,
                     player_render_.shadow_offset.y);
        LOG_INFO_APP("Characters: {} recovered; active {} ({}, controller {})",
                     player_characters_.Count(),
                     player_characters_.Active().id,
                     player_characters_.Active().controller_name,
                     player_characters_.Active().controller_path_id);
        for (std::size_t i = 0; i < player_characters_.Count(); ++i) {
            const auto& character = scene_.characters[i];
            LOG_INFO_APP("  character [{}] {}: body={} idle={} run={}",
                         i + 1, character.id, character.body_sprite_id,
                         character.idle_animation_id, character.run_animation_id);
        }
        std::string obstacle_error;
        if (!obstacles_.Configure(scene_, &obstacle_error)) {
            LOG_FATAL_APP("Obstacle configuration failed: {}", obstacle_error);
            return false;
        }
        LOG_INFO_APP("Obstacles: {} recovered entries, {} solid selectable, "
                     "{} markers, active {} ({} collider cells)",
                     scene_.obstacle_variants.size(),
                     obstacles_.Variants().size(), obstacles_.Markers().size(),
                     obstacles_.Variants().empty()
                         ? "none" : obstacles_.ActiveVariant().name,
                     obstacles_.ColliderCellCount());
        const int player_max_hp =
            scene_.player_stats.max_hp > 0 ? scene_.player_stats.max_hp : 6;
        const int player_max_armor =
            scene_.player_stats.max_armor > 0 ? scene_.player_stats.max_armor : 5;
        player_health_.Reset(player_max_hp, player_max_armor, 0);
        player_health_.camp =
            scene_.player_stats.max_hp > 0 ? scene_.player_stats.camp : 1;
        player_energy_.Reset(
            scene_.player_stats.max_energy > 0
                ? scene_.player_stats.max_energy : 180,
            scene_.player_stats.max_energy > 0
                ? scene_.player_stats.energy : 180);
        player_dead_ = false;
        aim_direction_ = {1.0f, 0.0f};
        mouse_aim_valid_ = true;
        projectiles_.Clear();
        std::string enemy_error;
        if (!enemies_.Configure(scene_, &enemy_error)) {
            LOG_FATAL_APP("Enemy configuration failed: {}", enemy_error);
            return false;
        }
        if (!weapons_.Configure(scene_)) {
            LOG_ERROR_APP("No recovered weapon definition; attack disabled");
        } else {
            game_ui_.SetWeaponCount(static_cast<int>(weapons_.Count()));
            for (const auto& definition : weapons_.Definitions()) {
                weapon_catalogue_.MarkRuntimeIntegrated(definition.id);
            }
            weapon_inventory_.Configure(weapons_.Count());
            // CONFIRMED GetInitWeaponName baseline. Scene characters carry the
            // recovered starting_weapon id from character_loadouts.json.
            if (player_characters_.Count() > 0 &&
                !player_characters_.Active().starting_weapon.empty()) {
                for (std::size_t i = 0; i < weapons_.Count(); ++i) {
                    if (weapons_.Definitions()[i].id ==
                        player_characters_.Active().starting_weapon) {
                        (void)weapon_inventory_.EquipActive(i);
                        break;
                    }
                }
            }
            ConfigureActiveWeapon();
            ConfigureSecondaryWeapon();
            ConfigureAbilities();
            // Stock seed must exist before the first stock roll. A fixed
            // --map-seed makes the stock deterministic; otherwise it is a
            // fresh random run seed persisted with the save.
            if (config_.map_seed_set) {
                trader_stock_seed_ = static_cast<std::int64_t>(map_seed_) ^
                    0x54524144;
            } else if (trader_stock_seed_ == 0) {
                std::random_device device;
                trader_stock_seed_ = static_cast<std::int64_t>(
                    (static_cast<std::uint64_t>(device()) << 32) ^
                    static_cast<std::uint64_t>(device()) ^
                    static_cast<std::uint64_t>(
                        std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
            }
            ConfigureTraderOffers();
            weapon_pickups_.Clear();
        }
        LOG_INFO_APP("Combat: player hp={} armor={} camp={}; enemies={} "
                     "(definitions={}), projectiles=0",
                     player_health_.hp, player_health_.armor,
                     player_health_.camp, enemies_.Enemies().size(),
                     enemies_.Definitions().size());
        if (scene_.is_hub) ConfigureHubGarden();
        if (config_.asset_lab) {
            asset_lab_.Open();
        }
        ConfigureRoomItems();
        boxes_.Configure(scene_, obstacles_);
        std::string pet_error;
        if (pet_.Configure(scene_, &pet_error)) {
            if (!active_pet_id_.empty()) pet_.SelectSaved(active_pet_id_);
            active_pet_id_ = pet_.SelectedId();
            LOG_INFO_APP("Pets: {} recovered, active={}", pet_.Count(),
                         active_pet_id_);
        } else {
            LOG_INFO_APP("Pets: {}", pet_error);
        }
        if (training_stake_.Configure(scene_)) {
            LOG_INFO_APP("Training stake: hp={} hit={:.3f}s",
                         scene_.training_stake.hp,
                         scene_.training_stake.hit_duration);
        }
        std::string door_error;
        if (!room_doors_.Configure(scene_, &door_error)) {
            LOG_FATAL_APP("Room door configuration failed: {}", door_error);
            return false;
        }
        SealUnconnectedDoors();
        door_trigger_inside_.assign(room_doors_.Count(), false);
        if (!scene_.is_hub && !scene_.doors.empty() &&
            enemies_.AliveCount() > 0) {
            room_doors_.CloseAll();
            room_clear_pending_ = true;
        }
        RebuildCollisionWorld();
        PopulateBiomeEnemies();
        room_entry_position_ = player_.Position();
        room_activation_timer_ = 0.0f;
        if (scene_.is_hub) {
            room_activation_state_ =
                soulknight::game::room::RoomActivationState::Inactive;
        } else if (enemies_.AliveCount() > 0 &&
                   !cleared_rooms_.contains(scene_.id)) {
            room_activation_state_ =
                soulknight::game::room::RoomActivationState::Entered;
        } else {
            room_activation_state_ =
                soulknight::game::room::RoomActivationState::Cleared;
        }
        LOG_INFO_APP("Room activation: {} state={}", scene_.id,
                     soulknight::game::room::RoomActivationName(
                         room_activation_state_));
        LOG_INFO_APP("Doors: {} recovered (open/close clips), initial target {}",
                     room_doors_.Count(),
                     room_doors_.TargetOpen() ? "open" : "closed");
        for (std::size_t i = 0; i < room_doors_.Count(); ++i) {
            const soulknight::game::room::Door& door = room_doors_.At(i);
            LOG_INFO_APP("  door {}: aisle={} barrier=[{:.2f},{:.2f}]..[{:.2f},{:.2f}] "
                         "open={}s close={}s state={}",
                         door.Data().id, door.Data().aisle,
                         door.Data().barrier.min.x, door.Data().barrier.min.y,
                         door.Data().barrier.max.x, door.Data().barrier.max.y,
                         door.Data().open_animation.duration,
                         door.Data().close_animation.duration,
                         soulknight::game::room::DoorStateName(door.State()));
        }
        LoadHubSuggestions();
        LOG_INFO_APP("Locomotion: spawn ({:.2f},{:.2f}), effective speed {:.2f}, "
                     "character={}, idle={}, run={}",
                     scene_.spawn.x, scene_.spawn.y, player_.Config().EffectiveSpeed(),
                     player_characters_.Active().id,
                     player_characters_.IdleClip() != nullptr ? "yes" : "no",
                     player_characters_.RunClip() != nullptr ? "yes" : "no");
        LOG_INFO_APP("Collision: bounds [{:.2f},{:.2f}]..[{:.2f},{:.2f}], "
                     "recovered solids={}, obstacle cells={}, doors={}, topology={}",
                     collision_world_.Bounds().min.x, collision_world_.Bounds().min.y,
                     collision_world_.Bounds().max.x, collision_world_.Bounds().max.y,
                     scene_.collision_solids.size(), obstacles_.ColliderCellCount(),
                     scene_.doors.size(), BooleanText(scene_.has_room_topology));
        if (scene_.interior_bounds.w > 0.0f && scene_.interior_bounds.h > 0.0f) {
            LOG_INFO_APP("Room interior: [{:.2f},{:.2f}]..[{:.2f},{:.2f}]",
                         scene_.interior_bounds.x, scene_.interior_bounds.y,
                         scene_.interior_bounds.x + scene_.interior_bounds.w,
                         scene_.interior_bounds.y + scene_.interior_bounds.h);
        }
        LOG_INFO_APP("Camera: center ({:.2f},{:.2f}), ortho size {:.2f}",
                     camera_.position.x, camera_.position.y,
                     camera_.orthographic_size);
        PlayMusic(scene_.is_hub ? "bgm_hub" : "bgm_dungeon");
    } else {
        const auto smoke_png = filesystem_.ReadFile(config_.texture_path);
        if (!smoke_png) {
            LOG_FATAL_APP("PNG not found in asset search path: {}", config_.texture_path);
            return false;
        }

        std::string png_error;
        if (!engine::assets::PngDecoder::Decode(smoke_png->data(), smoke_png->size(),
                                                smoke_texture_, &png_error)) {
            LOG_FATAL_APP("PNG decode failed: {}", png_error);
            return false;
        }
        LOG_INFO_APP("PNG decoded: {}, {}x{}, {} bytes",
                     config_.texture_path, smoke_texture_.width, smoke_texture_.height,
                     smoke_texture_.rgba8.size());
        texture_uploads_.push_back(engine::rendering::TextureUpload{
            0, &smoke_texture_,
            engine::assets::TextureFilter::Bilinear,
            engine::assets::TextureWrap::Clamp
        });
        camera_.position = {0.0f, 0.0f};
        camera_.orthographic_size = 10.0f;
    }

    const auto shader_dir = filesystem_.ResolvePath("shaders");
    if (shader_dir.empty()) {
        LOG_FATAL_APP("Shader directory not found in asset search path");
        return false;
    }
    LOG_INFO_APP("Shader directory: {}", shader_dir.string());

    platform::WindowConfig window_config;
    window_config.title = config_.title;
    window_config.width = config_.window_width;
    window_config.height = config_.window_height;
    window_config.fullscreen = config_.fullscreen;
    window_config.resizable = config_.resizable;
    window_config.high_dpi = config_.high_dpi;
    window_ = platform::Window(window_config);

    if (!window_.Initialize()) {
        LOG_FATAL_APP("Window initialization failed");
        return false;
    }
    LOG_INFO_APP("Window: {}x{} logical, {}x{} pixels",
                 window_.GetWidth(), window_.GetHeight(),
                 window_.GetPixelWidth(), window_.GetPixelHeight());

    if (!input_manager_.Initialize(window_.GetSDLWindow())) {
        LOG_FATAL_APP("Input manager initialization failed");
        return false;
    }

    renderer_ = std::make_unique<engine::rendering::VulkanRenderer>();
    engine::rendering::VulkanInitOptions render_options;
    render_options.enable_validation = config_.enable_validation;
    render_options.vsync = true;

    if (!renderer_->Initialize(window_, render_options, texture_uploads_, shader_dir)) {
        LOG_FATAL_APP("Vulkan renderer initialization failed");
        return false;
    }
    LOG_INFO_APP("Vulkan device: {}", renderer_->DeviceName());
    LOG_INFO_APP("Display refresh: {} Hz (simulation remains 50 Hz)",
                 window_.GetRefreshRate());
    LOG_INFO_APP("Vulkan validation: {}",
                 BooleanText(renderer_->ValidationEnabled()));

    BuildRenderBatch();
    LOG_INFO_APP("Render batch: {} sprite instances",
                 sprite_batch_.GetDraws().size());

    warmup_frames_ = 30;
    last_present_time_ = std::chrono::steady_clock::time_point{};
    present_window_sum_ms_ = 0.0;
    present_window_min_ms_ = 0.0;
    present_window_max_ms_ = 0.0;
    present_window_samples_ = 0;
    present_run_sum_ms_ = 0.0;
    present_run_samples_ = 0;
    perf_samples_ = 0;
    perf_total_frame_ms_ = 0.0;
    perf_total_sim_steps_ = 0;
    simulation_time_ = 0.0;
    simulation_ticks_ = 0;
    render_frames_ = 0;
    exercise_stage_ = 0;

    initialized_ = true;
    running_ = true;
    smoke_start_seconds_ = NowSeconds();

    if (config_.start_gui) {
        game_ui_.OpenMainMenu();
        main_menu_started_ = true;
    }
    ConfigureReviewMode();
    if (config_.interactive_test) {
        StartInteractiveTest();
    }
    if (config_.replay && replay_ != nullptr) {
        StartReplaySession();
    }
    LOG_INFO_APP("Application started");
    if (config_.smoke_test_seconds > 0.0) {
        LOG_INFO_APP("Smoke test will run for {:.1f} seconds", config_.smoke_test_seconds);
    }
    if (config_.exercise_window) {
        LOG_INFO_APP("Window exercise enabled: resize, minimize, restore, fail-safe");
    }
    return true;
}

void Application::RefreshLevelSurfaces() {
    const soulknight::game::room::BiomeLevel* level = biome_.Current();
    if (level == nullptr) {
        level_surfaces_.clear();
        level_surface_key_.clear();
        return;
    }
    std::string key = scene_.id + "|" + level->id + "|" +
                      std::to_string(map_seed_);
    if (key == level_surface_key_ && !level_surfaces_.empty()) return;
    level_surfaces_.clear();
    level_surface_key_ = key;
    const engine::assets::LevelTheme* theme =
        soulknight::game::room::LevelSurfaceBuilder::Find(
            scene_, level->id, "A");
    active_level_theme_ = theme;
    if (theme == nullptr) return;
    std::uint32_t seed = 2166136261u;
    for (const char c : key) {
        seed ^= static_cast<unsigned char>(c);
        seed *= 16777619u;
    }
    level_surfaces_ = soulknight::game::room::LevelSurfaceBuilder::Build(
        scene_, *theme, seed);
    LOG_INFO_APP("Level surfaces: {} theme={} cells floor={} walls={}",
                 level->id, theme->manager, scene_.floor_cells.size(),
                 scene_.wall_cells.size() + scene_.corridor_wall_cells.size());
    if (!level_surfaces_.empty()) {
    }
}

void Application::BuildRenderBatch() {
    sprite_batch_.Clear();
    if (!scene_mode_) {
        BuildSyntheticBatch();
        return;
    }

    RefreshLevelSurfaces();
    const bool themed_surfaces = !level_surfaces_.empty();
    if (locomotion_animator_.State() != last_animation_state_) {
        last_animation_state_ = locomotion_animator_.State();
        animation_logs_remaining_ = 8;
    }

    const std::string active_character_prefix =
        player_characters_.Configured()
        ? "characters/" + player_characters_.Active().id + "/"
        : std::string{};
    for (std::size_t scene_index = 0; scene_index < scene_.instances.size();
         ++scene_index) {
        const engine::assets::SpriteInstance& instance =
            scene_.instances[scene_index];
        if (themed_surfaces &&
            instance.path.rfind("native_wall/", 0) == 0) {
            continue;
        }
        if (instance.sprite_index >= scene_.sprites.size()) continue;
        // Do not render a second copy of the selected player as a hub NPC.
        if (!active_character_prefix.empty() &&
            instance.path.rfind(active_character_prefix, 0) == 0) {
            continue;
        }
        const int32_t instance_index = static_cast<int32_t>(scene_index);
        soulknight::game::room::DoorVisualState door_visuals;
        bool door_visual_override = false;
        if (!instance.door.empty()) {
            const int32_t door_index = scene_.FindDoorIndex(instance.door);
            if (door_index >= 0 &&
                static_cast<std::size_t>(door_index) < room_doors_.Count()) {
                const soulknight::game::room::Door& door =
                    room_doors_.At(static_cast<std::size_t>(door_index));
                if (!door.IsEnabled()) continue;
                door_visuals = door.Visuals();
                door_visual_override = true;
                if (instance.door_role == "shadow" &&
                    !door_visuals.shadow_visible) {
                    continue;
                }
                if (instance.door_role == "ornament" &&
                    !door_visuals.ornament_visible) {
                    continue;
                }
            } else {
                continue;
            }
        }
        const engine::assets::SpriteData* sprite =
            &scene_.sprites[instance.sprite_index];
        const bool is_player_body =
            instance_index == player_render_.body_instance;
        const bool is_stake_body = scene_.has_training_stake &&
            instance.path == "room/objects/stake/img/body/SpriteRenderer";
        if (is_stake_body) {
            const int32_t stake_sprite = training_stake_.CurrentSprite();
            if (stake_sprite >= 0 &&
                static_cast<std::size_t>(stake_sprite) < scene_.sprites.size()) {
                sprite = &scene_.sprites[
                    static_cast<std::size_t>(stake_sprite)];
            }
        }
        if (!is_player_body &&
            instance.path.rfind("characters/", 0) == 0 &&
            instance.path.find("/img/body") != std::string::npos) {
            const std::size_t slash = instance.path.find('/', 11);
            if (slash != std::string::npos) {
                const std::string character_id =
                    instance.path.substr(11, slash - 11);
                const std::string clip_id = character_id + "_idle";
                for (const engine::animation::AnimationClip& clip :
                     scene_.animations) {
                    if (clip.id != clip_id) continue;
                    const engine::animation::SpriteTrack* track =
                        clip.FindTrack(instance.path);
                    if (track == nullptr) break;
                    const float offset = static_cast<float>(
                        std::hash<std::string>{}(character_id) % 1000) * 0.001f;
                    const float time = clip.duration > 0.0f
                        ? static_cast<float>(std::fmod(
                              simulation_time_ + offset,
                              static_cast<double>(clip.duration)))
                        : 0.0f;
                    const engine::animation::SpriteKeyframe* key =
                        track->Evaluate(time);
                    if (key != nullptr) {
                        const int32_t animated_index =
                            scene_.FindSpriteIndex(key->sprite_id);
                        if (animated_index >= 0 &&
                            static_cast<std::size_t>(animated_index) <
                                scene_.sprites.size()) {
                            sprite = &scene_.sprites[
                                static_cast<std::size_t>(animated_index)];
                        }
                    }
                    break;
                }
            }
        }

        if (is_player_body && player_characters_.Count() > 0) {
            const engine::assets::SceneCharacter& character =
                player_characters_.Active();
            const int32_t character_sprite =
                player_dead_ ? character.dead_sprite_index
                             : character.body_sprite_index;
            if (character_sprite >= 0 &&
                static_cast<std::size_t>(character_sprite) <
                    scene_.sprites.size()) {
                sprite = &scene_.sprites[
                    static_cast<std::size_t>(character_sprite)];
            }
        }
        if (door_visual_override && instance.door_role == "leaf" &&
            !door_visuals.leaf_sprite_id.empty()) {
            const int32_t animated_index =
                scene_.FindSpriteIndex(door_visuals.leaf_sprite_id);
            if (animated_index >= 0 &&
                static_cast<std::size_t>(animated_index) < scene_.sprites.size()) {
                sprite = &scene_.sprites[static_cast<std::size_t>(animated_index)];
            }
        }
        if (locomotion_animator_.IsPlaying()) {
            const engine::animation::SpriteKeyframe* key =
                locomotion_animator_.Evaluate(instance.path);
            if (key != nullptr) {
                const int32_t animated_index = scene_.FindSpriteIndex(key->sprite_id);
                if (animated_index >= 0 &&
                    static_cast<std::size_t>(animated_index) < scene_.sprites.size()) {
                    sprite = &scene_.sprites[static_cast<std::size_t>(animated_index)];
                }
                if (key->sprite_id != last_animation_sprite_) {
                    if (animation_logs_remaining_ > 0) {
                        LOG_INFO_APP("Animation frame: {} at t={:.3f}s state={}",
                                     key->sprite_id, locomotion_animator_.Time(),
                                     engine::animation::LocomotionStateName(
                                         locomotion_animator_.State()));
                        --animation_logs_remaining_;
                    }
                    last_animation_sprite_ = key->sprite_id;
                }
            }
        }
        engine::rendering::SpriteDraw draw;
        draw.position = engine::rendering::ResolvePlayerRenderPosition(
            scene_, player_render_, instance_index, render_player_position_);
        if (is_stake_body) {
            draw.position += training_stake_.RenderOffset();
        }
        if (door_visual_override && instance.door_role == "ornament") {
            // The ornament instance is authored at leaf + (0,1). The clip
            // animates the child's local y between 0.1 and 1.
            draw.position.y += door_visuals.ornament_extent - 1.0f;
        }
        draw.size = engine::Vec2{
            sprite->WorldSize().x * instance.scale.x,
            sprite->WorldSize().y * instance.scale.y
        };
        draw.uv = sprite->UvRect();
        draw.color = instance.color;
        const float foot_y = engine::rendering::SpriteFootY(
            draw.position.y, draw.size.y, sprite->pivot.y);
        if (engine::rendering::IsCharacterSortedUnityLayer(
                instance.sorting_layer) || instance.ground_depth) {
            draw.layer = engine::rendering::SortLayer::Character;
            draw.sorting_order = engine::rendering::WorldDepthOrder(
                foot_y, is_player_body ? 20 : 0);
        } else {
            draw.layer = MapSortingLayer(instance.sorting_layer);
            draw.sorting_order = instance.sorting_order;
        }
        draw.texture_id = sprite->texture_id;
        draw.pivot = sprite->pivot;
        draw.rotation = instance.rotation;
        draw.flip_x = instance.flip_x;
        if (is_stake_body && training_stake_.HitFlash()) {
            draw.color = engine::Color{255, 235, 215, 255};
        }
        if (is_player_body && stealth_timer_ > 0.0f) {
            draw.color.a = 76; // BuffStealth serialized alpha 0.297
        }
        if (is_player_body && hit_flash_timer_ > 0.0f) {
            const bool bright = static_cast<int>(hit_flash_timer_ * 100.0f) % 2 == 0;
            draw.color = bright ? engine::Color{255, 245, 235, 255}
                                : engine::Color{255, 150, 150, 255};
        }
        if (is_player_body) {
            draw.flip_x = engine::rendering::ResolvePlayerBodyFlipX(
                instance.flip_x, facing_);
        } else if (instance.path.rfind("characters/", 0) == 0 &&
                   instance.path.find("/img/body") != std::string::npos &&
                   (player_.Position() - instance.position)
                           .LengthSquared() <= 16.0f) {
            draw.flip_x = instance.flip_x !=
                (player_.Position().x < instance.position.x);
        }
        draw.flip_y = instance.flip_y;
        (void)sprite_batch_.DrawSprite(draw);
    }

    for (const soulknight::game::room::SurfaceInstance& surface :
         level_surfaces_) {
        if (surface.sprite_index < 0 ||
            static_cast<std::size_t>(surface.sprite_index) >=
                scene_.sprites.size()) {
            continue;
        }
        const engine::assets::SpriteData& sprite =
            scene_.sprites[static_cast<std::size_t>(surface.sprite_index)];
        engine::rendering::SpriteDraw draw;
        draw.position = surface.position;
        draw.size = engine::Vec2{sprite.WorldSize().x * surface.scale.x,
                                 sprite.WorldSize().y * surface.scale.y};
        draw.uv = sprite.UvRect();
        draw.color = surface.color;
        draw.layer = MapSortingLayer(surface.sorting_layer);
        draw.sorting_order = surface.sorting_order;
        draw.texture_id = sprite.texture_id;
        draw.pivot = sprite.pivot;
        draw.rotation = surface.rotation;
        draw.flip_x = surface.flip_x;
        draw.flip_y = surface.flip_y;
        (void)sprite_batch_.DrawSprite(draw);
    }

    if (pet_.Count() > 0) {
        const auto draw_pet_sprite = [&](int32_t sprite_index,
                                          const engine::Vec2& position,
                                          bool flip_x, int32_t tie_break) {
            if (sprite_index < 0 ||
                static_cast<std::size_t>(sprite_index) >= scene_.sprites.size()) {
                return;
            }
            const engine::assets::SpriteData& sprite =
                scene_.sprites[static_cast<std::size_t>(sprite_index)];
            engine::rendering::SpriteDraw draw;
            draw.position = position;
            draw.size = sprite.WorldSize();
            draw.uv = sprite.UvRect();
            draw.color = engine::Color::White();
            draw.layer = engine::rendering::SortLayer::Character;
            draw.sorting_order = engine::rendering::WorldDepthOrder(
                position.y, tie_break);
            draw.texture_id = sprite.texture_id;
            draw.pivot = sprite.pivot;
            draw.flip_x = flip_x;
            (void)sprite_batch_.DrawSprite(draw);
        };
        draw_pet_sprite(pet_.ShadowSprite(), pet_.Position(),
                        pet_.FacingLeft(), 1);
        draw_pet_sprite(pet_.CurrentSprite(), pet_.Position(),
                        pet_.FacingLeft(), 6);
    }

    if (!obstacles_.Variants().empty()) {
        for (std::size_t marker_index = 0;
             marker_index < obstacles_.Markers().size(); ++marker_index) {
            if (!obstacles_.MarkerAlive(marker_index)) continue;
            const engine::assets::ObstacleMarker& marker =
                obstacles_.Markers()[marker_index];
            const engine::assets::ObstacleVariant& obstacle_variant =
                obstacles_.VariantForMarker(marker);
            bool box_hit_flash = false;
            for (const auto& box : boxes_.Boxes()) {
                if (box.marker_index == marker_index && box.hit_flash > 0.0f) {
                    box_hit_flash = true;
                    break;
                }
            }
            bool themed_obstacle = false;
            if (active_level_theme_ != nullptr &&
                marker.variant >= 0 &&
                static_cast<std::size_t>(marker.variant) <
                    active_level_theme_->obstacles.size()) {
                const auto& group =
                    active_level_theme_->obstacles[
                        static_cast<std::size_t>(marker.variant)];
                for (const engine::assets::LevelSurfaceSprite& part :
                     group.sprites) {
                    if (part.sprite_index < 0 ||
                        static_cast<std::size_t>(part.sprite_index) >=
                            scene_.sprites.size()) {
                        continue;
                    }
                    const engine::assets::SpriteData& sprite =
                        scene_.sprites[
                            static_cast<std::size_t>(part.sprite_index)];
                    engine::rendering::SpriteDraw draw;
                    draw.position = marker.position + part.offset;
                    draw.size = engine::Vec2{
                        sprite.WorldSize().x * part.scale.x,
                        sprite.WorldSize().y * part.scale.y};
                    draw.uv = sprite.UvRect();
                    draw.color = part.color;
                    draw.layer = MapSortingLayer(part.sorting_layer);
                    draw.sorting_order = part.sorting_order;
                    draw.texture_id = sprite.texture_id;
                    draw.pivot = sprite.pivot;
                    draw.rotation = part.rotation;
                    draw.flip_x = part.flip_x;
                    draw.flip_y = part.flip_y;
                    (void)sprite_batch_.DrawSprite(draw);
                    themed_obstacle = true;
                }
            }
            if (themed_obstacle) continue;
            for (const engine::assets::ObstacleVisual& visual :
                 obstacle_variant.visuals) {
                if (visual.sprite_index < 0 ||
                    static_cast<std::size_t>(visual.sprite_index) >=
                        scene_.sprites.size()) {
                    continue;
                }
                const engine::assets::SpriteData& sprite =
                    scene_.sprites[static_cast<std::size_t>(visual.sprite_index)];
                engine::rendering::SpriteDraw draw;
                draw.position = marker.position + visual.offset;
                draw.size = engine::Vec2{
                    sprite.WorldSize().x * visual.scale.x,
                    sprite.WorldSize().y * visual.scale.y
                };
                draw.uv = sprite.UvRect();
                draw.color = box_hit_flash
                    ? engine::Color{255, 235, 215, 255} : visual.color;
                if (engine::rendering::IsCharacterSortedUnityLayer(
                        visual.sorting_layer)) {
                    draw.layer = engine::rendering::SortLayer::Character;
                    draw.sorting_order = engine::rendering::WorldDepthOrder(
                        draw.position.y);
                } else {
                    draw.layer = MapSortingLayer(visual.sorting_layer);
                    draw.sorting_order = visual.sorting_order;
                }
                draw.texture_id = sprite.texture_id;
                draw.pivot = sprite.pivot;
                draw.rotation = visual.rotation;
                draw.flip_x = visual.flip_x;
                draw.flip_y = visual.flip_y;
                (void)sprite_batch_.DrawSprite(draw);
            }
        }
    }

    for (const auto& box : boxes_.Boxes()) {
        if (!box.alive ||
            box.marker_index != soulknight::game::items::BreakableBox::kDeployedMarker ||
            box.sprite_index < 0 ||
            static_cast<std::size_t>(box.sprite_index) >= scene_.sprites.size()) {
            continue;
        }
        const engine::assets::SpriteData& sprite =
            scene_.sprites[static_cast<std::size_t>(box.sprite_index)];
        engine::rendering::SpriteDraw draw;
        draw.position = box.position;
        draw.size = sprite.WorldSize();
        draw.uv = sprite.UvRect();
        draw.color = box.hit_flash > 0.0f
            ? engine::Color{255, 235, 215, 255} : engine::Color::White();
        draw.layer = engine::rendering::SortLayer::Character;
        draw.sorting_order = engine::rendering::WorldDepthOrder(
            box.position.y, 4);
        draw.texture_id = sprite.texture_id;
        draw.pivot = sprite.pivot;
        (void)sprite_batch_.DrawSprite(draw);
    }

    if (item_assets_.IsLoaded()) {
        const auto draw_item_sprite = [&](const std::string& sprite_id,
                                          const engine::Vec2& position,
                                          int32_t unity_layer,
                                          int32_t fallback_order,
                                          int32_t tie_break) {
            const int32_t sprite_index = scene_.FindSpriteIndex(sprite_id);
            if (sprite_index < 0 ||
                static_cast<std::size_t>(sprite_index) >= scene_.sprites.size()) {
                return;
            }
            const engine::assets::SpriteData& sprite =
                scene_.sprites[static_cast<std::size_t>(sprite_index)];
            engine::rendering::SpriteDraw draw;
            draw.position = position;
            draw.size = sprite.WorldSize();
            draw.uv = sprite.UvRect();
            draw.color = engine::Color::White();
            draw.texture_id = sprite.texture_id;
            draw.pivot = sprite.pivot;
            if (engine::rendering::IsCharacterSortedUnityLayer(unity_layer)) {
                draw.layer = engine::rendering::SortLayer::Character;
                draw.sorting_order = engine::rendering::WorldDepthOrder(
                    position.y, tie_break);
            } else {
                draw.layer = MapSortingLayer(unity_layer);
                draw.sorting_order = fallback_order;
            }
            (void)sprite_batch_.DrawSprite(draw);
        };
        if (const auto* energy = item_assets_.EnergyPickup(); energy != nullptr) {
            const int32_t sprite_index = scene_.FindSpriteIndex(energy->sprite);
            if (sprite_index >= 0 &&
                static_cast<std::size_t>(sprite_index) < scene_.sprites.size()) {
                const engine::assets::SpriteData& sprite =
                    scene_.sprites[static_cast<std::size_t>(sprite_index)];
                for (const auto& bubble : boxes_.Bubbles()) {
                    if (!bubble.active) continue;
                    engine::rendering::SpriteDraw draw;
                    draw.position = bubble.position + energy->offset;
                    draw.size = sprite.WorldSize();
                    draw.uv = sprite.UvRect();
                    draw.color = engine::Color::White();
                    draw.layer = engine::rendering::SortLayer::Character;
                    draw.sorting_order = engine::rendering::WorldDepthOrder(
                        draw.position.y, 6);
                    draw.texture_id = sprite.texture_id;
                    draw.pivot = sprite.pivot;
                    (void)sprite_batch_.DrawSprite(draw);
                }
            }
        }
        for (const auto& chest : item_world_.Chests()) {
            const auto* definition = item_assets_.FindChest(chest.definition);
            if (definition == nullptr) continue;
            const float lift = chest.state == soulknight::game::items::ChestState::Opening
                ? std::min(chest.timer / 0.45f, 1.0f) * 0.32f : 0.0f;
            for (std::size_t i = 0; i < definition->visuals.size(); ++i) {
                const auto& visual = definition->visuals[i];
                if (chest.state == soulknight::game::items::ChestState::Open &&
                    i == 0) {
                    continue; // lid is removed after opening
                }
                engine::Vec2 position = chest.position + visual.offset;
                if (i == 0) position.y += lift;
                draw_item_sprite(visual.sprite, position,
                                 visual.sorting_layer, visual.sorting_order, 4);
            }
        }
        if (scene_.is_hub) {
            // Recovered Living Room seed stock: the initial seed is a marked
            // native grant, but every plant sprite/state comes from the
            // recovered plant_* prefabs.
            for (const auto& plot : plant_system_.Plots()) {
                if (plot.state == game::hub::PlantGrowthState::Empty ||
                    plot.species.empty()) {
                    continue;
                }
                const auto* definition = item_assets_.FindPlant(plot.species);
                if (definition == nullptr || definition->states.empty()) {
                    continue;
                }
                std::size_t visual_index = definition->states.size() - 1;
                if (plot.state == game::hub::PlantGrowthState::Growing) {
                    const float progress = plot.growth_time > 0.0f
                        ? std::clamp(plot.elapsed / plot.growth_time,
                                     0.0f, 1.0f)
                        : 1.0f;
                    visual_index = static_cast<std::size_t>(
                        progress * static_cast<float>(
                            definition->states.size() - 1));
                    visual_index = std::min(
                        visual_index, definition->states.size() - 2);
                }
                const auto& visual = definition->states[visual_index];
                const int32_t sprite_index =
                    scene_.FindSpriteIndex(visual.sprite);
                if (sprite_index < 0 ||
                    static_cast<std::size_t>(sprite_index) >=
                        scene_.sprites.size()) {
                    continue;
                }
                const auto& sprite = scene_.sprites[
                    static_cast<std::size_t>(sprite_index)];
                engine::rendering::SpriteDraw draw;
                draw.position = plot.position + visual.offset;
                draw.size = sprite.WorldSize();
                draw.uv = sprite.UvRect();
                draw.color = engine::Color::White();
                draw.layer = engine::rendering::SortLayer::Character;
                // Plants grow out of the soil above the planter's front lip.
                // Sort from the planter face, not the root, so the lip does
                // not hide the whole recovered plant prefab.
                draw.sorting_order = engine::rendering::WorldDepthOrder(
                    plot.position.y - 2.3f, 9);
                draw.texture_id = sprite.texture_id;
                draw.pivot = sprite.pivot;
                (void)sprite_batch_.DrawSprite(draw);
            }
            // The three recovered seller display cases in front of the trader
            // show the actual weapon sprites. A sold offer leaves its case
            // empty, which is the visible sold state.
            const engine::Vec2 case_positions[] = {
                {24.6f, 5.95f}, {27.6f, 5.95f}, {30.6f, 5.95f}};
            for (std::size_t i = 0; i < trader_offers_.size() &&
                                    i < std::size(case_positions); ++i) {
                const auto& offer = trader_offers_[i];
                if (offer.sold || offer.sprite_index < 0 ||
                    static_cast<std::size_t>(offer.sprite_index) >=
                        scene_.sprites.size()) {
                    continue;
                }
                const auto& sprite = scene_.sprites[
                    static_cast<std::size_t>(offer.sprite_index)];
                engine::rendering::SpriteDraw draw;
                const engine::Vec2 world = sprite.WorldSize();
                const float base_y = 5.91f;
                draw.position = {
                    case_positions[i].x -
                        (0.5f - sprite.pivot.y) * world.y,
                    base_y + (1.0f - sprite.pivot.x) * world.x};
                draw.size = world;
                draw.uv = sprite.UvRect();
                draw.color = engine::Color::White();
                draw.layer = engine::rendering::SortLayer::Character;
                draw.sorting_order = engine::rendering::WorldDepthOrder(
                    base_y, 11);
                draw.texture_id = sprite.texture_id;
                draw.pivot = sprite.pivot;
                draw.rotation = -3.14159265358979323846f * 0.5f;
                (void)sprite_batch_.DrawSprite(draw);
            }
        }
        if (scene_.id == "r_sell_visual" && seller_.Configured()) {
            const auto& visuals = item_assets_.SellerVisuals();
            const engine::Vec2 base = scene_.spawn;
            if (!visuals.counter.empty()) {
                draw_item_sprite(visuals.counter,
                                 base + engine::Vec2{0.0f, 3.05f}, 4, 2, 2);
            }
            if (!visuals.npc_shadow.empty()) {
                draw_item_sprite(visuals.npc_shadow,
                                 base + engine::Vec2{0.0f, 3.15f}, 3, 0, 1);
            }
            if (!visuals.npc_body.empty()) {
                draw_item_sprite(visuals.npc_body,
                                 base + engine::Vec2{0.0f, 3.45f}, 5, 1, 3);
            }
            if (!visuals.pedestal.empty()) {
                for (const auto& offer : seller_.Offers()) {
                    draw_item_sprite(visuals.pedestal,
                                     offer.position + engine::Vec2{0.0f, -0.35f},
                                     4, 0, 2);
                }
            }
        }
        for (const auto& offer : seller_.Offers()) {
            if (offer.sold || offer.sprite.empty()) continue;
            draw_item_sprite(offer.sprite,
                             offer.position + engine::Vec2{0.0f, 0.62f},
                             5, 5, 7);
        }
        for (const auto& object : special_objects_) {
            if (!object.active || object.used || object.sprite.empty()) continue;
            draw_item_sprite(object.sprite,
                             object.position + engine::Vec2{0.0f, 0.7f},
                             5, 5, 7);
        }
        for (const auto& coin : item_world_.Coins()) {
            if (!coin.active) continue;
            const auto* definition = item_assets_.FindCoin(coin.definition);
            if (definition == nullptr || definition->frames.empty()) continue;
            const std::size_t frame = static_cast<std::size_t>(
                static_cast<int>(coin.age * 8.0f) %
                static_cast<int>(definition->frames.size()));
            draw_item_sprite(definition->frames[frame], coin.position,
                             5, 5, 8);
        }
    }

    for (const soulknight::game::summon::SummonInstance& summon :
         summons_.Summons()) {
        if (!summon.active || summon.sprite_index < 0 ||
            static_cast<std::size_t>(summon.sprite_index) >=
                scene_.sprites.size()) {
            continue;
        }
        const engine::assets::SpriteData& sprite =
            scene_.sprites[static_cast<std::size_t>(summon.sprite_index)];
        engine::rendering::SpriteDraw draw;
        draw.position = summon.position;
        draw.size = sprite.WorldSize();
        draw.uv = sprite.UvRect();
        draw.color = engine::Color::White();
        draw.layer = engine::rendering::SortLayer::Character;
        draw.sorting_order = engine::rendering::WorldDepthOrder(
            summon.position.y, 12);
        draw.texture_id = sprite.texture_id;
        draw.pivot = sprite.pivot;
        draw.flip_x = summon.facing_left;
        (void)sprite_batch_.DrawSprite(draw);
    }

    for (const soulknight::game::enemy::EnemyInstance& enemy :
         enemies_.Enemies()) {
        if (!enemy.active) continue;
        const engine::assets::EnemyDefinition& definition =
            enemies_.Definitions()[enemy.definition];
        const int32_t sprite_index =
            enemy.health.dead ? definition.dead_sprite_index
                              : definition.body_sprite_index;
        if (sprite_index >= 0 &&
            static_cast<std::size_t>(sprite_index) < scene_.sprites.size()) {
            const engine::assets::SpriteData& sprite =
                scene_.sprites[static_cast<std::size_t>(sprite_index)];
            engine::rendering::SpriteDraw draw;
            draw.position = enemy.position;
            draw.size = sprite.WorldSize();
            draw.uv = sprite.UvRect();
            draw.color = engine::Color::White();
            if (engine::rendering::IsCharacterSortedUnityLayer(
                    definition.body_sorting_layer)) {
                draw.layer = engine::rendering::SortLayer::Character;
                draw.sorting_order = engine::rendering::WorldDepthOrder(
                    enemy.position.y);
            } else {
                draw.layer = MapSortingLayer(definition.body_sorting_layer);
                draw.sorting_order = definition.body_sorting_order;
            }
            draw.texture_id = sprite.texture_id;
            draw.pivot = sprite.pivot;
            draw.flip_x = enemy.facing_left;
            (void)sprite_batch_.DrawSprite(draw);
        }
        if (definition.shadow_sprite_index >= 0 &&
            static_cast<std::size_t>(definition.shadow_sprite_index) <
                scene_.sprites.size()) {
            const engine::assets::SpriteData& sprite =
                scene_.sprites[static_cast<std::size_t>(
                    definition.shadow_sprite_index)];
            engine::rendering::SpriteDraw draw;
            draw.position = enemy.position;
            draw.size = sprite.WorldSize();
            draw.uv = sprite.UvRect();
            draw.color = engine::Color::White();
            draw.layer = MapSortingLayer(definition.shadow_sorting_layer);
            draw.sorting_order = definition.shadow_sorting_order;
            draw.texture_id = sprite.texture_id;
            draw.pivot = sprite.pivot;
            (void)sprite_batch_.DrawSprite(draw);
        }
    }

    for (const soulknight::game::weapons::WeaponPickup& pickup :
         weapon_pickups_.Pickups()) {
        if (!pickup.active || pickup.weapon_index >= scene_.weapons.size()) continue;
        const engine::assets::WeaponDefinition& definition =
            scene_.weapons[pickup.weapon_index];
        if (definition.visual_sprite_index < 0 ||
            static_cast<std::size_t>(definition.visual_sprite_index) >=
                scene_.sprites.size()) {
            continue;
        }
        const engine::assets::SpriteData& sprite =
            scene_.sprites[static_cast<std::size_t>(definition.visual_sprite_index)];
        engine::rendering::SpriteDraw draw;
        const float bob = std::sin(pickup.animation_time * 3.0f) * 0.08f;
        draw.position = pickup.position + engine::Vec2{0.0f, bob};
        draw.size = sprite.WorldSize();
        draw.uv = sprite.UvRect();
        draw.color = engine::Color::White();
        draw.layer = engine::rendering::SortLayer::Character;
        draw.sorting_order = engine::rendering::WorldDepthOrder(
            pickup.position.y, 10);
        draw.texture_id = sprite.texture_id;
        draw.pivot = sprite.pivot;
        (void)sprite_batch_.DrawSprite(draw);
    }

    for (const soulknight::game::combat::Projectile& projectile :
         projectiles_.Projectiles()) {
        if (!projectile.active || projectile.sprite_index < 0 ||
            static_cast<std::size_t>(projectile.sprite_index) >=
                scene_.sprites.size()) {
            continue;
        }
        const engine::assets::SpriteData& sprite =
            scene_.sprites[static_cast<std::size_t>(projectile.sprite_index)];
        engine::rendering::SpriteDraw draw;
        draw.position = projectile.position;
        draw.size = sprite.WorldSize() * weapon_.Definition().size;
        draw.uv = sprite.UvRect();
        draw.color = engine::Color::White();
        draw.layer = MapSortingLayer(
            weapon_.Definition().projectile.sorting_layer);
        draw.sorting_order = weapon_.Definition().projectile.sorting_order;
        draw.texture_id = sprite.texture_id;
        draw.pivot = sprite.pivot;
        draw.rotation = std::atan2(projectile.velocity.y,
                                   projectile.velocity.x);
        (void)sprite_batch_.DrawSprite(draw);
    }

    if (mech_station_open_) {
        auto& text = dev_ui_.Text();
        const engine::Vec2 panel_size{18.0f, 10.6f};
        const engine::Vec2 origin{
            camera_.position.x - panel_size.x * 0.5f,
            camera_.position.y + 5.30f};
        text.DrawPanelBorder(sprite_batch_, origin, panel_size,
                             {18, 24, 36, 238}, {188, 150, 74, 255},
                             0.035f, 1500);
        text.DrawText(sprite_batch_, "ROBOTICS FACILITY",
                      origin + engine::Vec2{0.75f, -0.60f}, 0.015f,
                      {255, 220, 145, 255}, 1502);
        text.DrawText(sprite_batch_,
                      "ORBS: " + std::to_string(player_orbs_) +
                          "   ACTIVE: " +
                          (active_mount_id_.empty() ? "NONE" : active_mount_id_),
                      origin + engine::Vec2{0.75f, -1.35f}, 0.0105f,
                      {225, 235, 245, 255}, 1502);
        for (std::size_t i = 0; i < std::size(kMechOffers); ++i) {
            const MechOffer& offer = kMechOffers[i];
            const bool selected = static_cast<int>(i) == mech_selected_;
            const bool active = active_mount_id_ == offer.id;
            const bool locked = offer.unlock_key[0] != '\0';
            std::string line = std::string(selected ? "> " : "  ") + offer.name;
            line += "   " + std::to_string(offer.cost) + " ORBS";
            line += "  DEF " + std::to_string(offer.defence);
            if (locked) line += "  BLUEPRINT";
            if (active) line += "  [ACTIVE]";
            text.DrawText(sprite_batch_, line,
                          origin + engine::Vec2{0.75f,
                              -2.10f - static_cast<float>(i) * 0.78f},
                          0.0115f,
                          selected ? engine::Color{255, 220, 145, 255}
                                   : engine::Color{190, 205, 220, 255},
                          1502);
        }
        if (!mech_status_.empty()) {
            text.DrawText(sprite_batch_, mech_status_,
                          origin + engine::Vec2{0.75f, -7.30f}, 0.0125f,
                          {150, 235, 185, 255}, 1502);
        }
        text.DrawText(sprite_batch_,
                      "UP/DOWN SELECT   ENTER BUILD/STORE   ESC CLOSE",
                      origin + engine::Vec2{0.75f, -8.40f}, 0.0095f,
                      {200, 210, 225, 255}, 1502);
    }

    DrawHubSuggestionOverlay();
    if (calibration_mode_) DrawCalibrationOverlay();

    if (collision_debug_) DrawCollisionDebug();

    if (weapon_.IsConfigured()) {
        soulknight::game::weapons::WeaponRenderer::Draw(
            sprite_batch_, scene_, weapon_.Definition(), player_.Position(),
            render_aim_direction_,
            facing_ == engine::locomotion::FacingDirection::Left,
            player_dead_, weapon_.VisualTime(), weapon_.IsAttacking(),
            reduced_weapon_effects_ ? 0.22f : 1.0f);
        const bool dual_active = abilities_.Count() > 0 &&
            abilities_.Abilities()[0].state ==
                soulknight::game::player::AbilityState::Active;
        if (dual_active && secondary_weapon_.IsConfigured()) {
            const float mirror =
                facing_ == engine::locomotion::FacingDirection::Left
                ? -1.0f : 1.0f;
            soulknight::game::weapons::WeaponRenderer::DrawAt(
                sprite_batch_, scene_, secondary_weapon_.Definition(),
                player_.Position(), render_aim_direction_,
                {0.55f * mirror, 0.6f}, player_dead_,
                secondary_weapon_.VisualTime(),
                secondary_weapon_.IsAttacking(),
                reduced_weapon_effects_ ? 0.22f : 1.0f);
        }
    }

    if (weapon_.IsCharging() &&
        !weapon_.Definition().special.sprite_id.empty()) {
        const int32_t sprite_index = scene_.FindSpriteIndex(
            weapon_.Definition().special.sprite_id);
        if (sprite_index >= 0 &&
            static_cast<std::size_t>(sprite_index) < scene_.sprites.size()) {
            const engine::assets::SpriteData& sprite =
                scene_.sprites[static_cast<std::size_t>(sprite_index)];
            const float progress = weapon_.ChargePercent();
            const int filled = std::clamp(
                static_cast<int>(std::ceil(progress * 5.0f)), 0, 5);
            for (int segment = 0; segment < 5; ++segment) {
                engine::rendering::SpriteDraw draw;
                draw.position = player_.Position() +
                    engine::Vec2{-0.8f + static_cast<float>(segment) * 0.32f,
                                 1.75f};
                draw.size = sprite.WorldSize() * 0.8f;
                draw.uv = sprite.UvRect();
                draw.color = segment < filled
                    ? engine::Color::White()
                    : engine::Color{120, 120, 120, 90};
                draw.layer = engine::rendering::SortLayer::Effect;
                draw.sorting_order = 40;
                draw.texture_id = sprite.texture_id;
                draw.pivot = sprite.pivot;
                (void)sprite_batch_.DrawSprite(draw);
            }
        }
    }

    // Actual transfer_gate_02 sprite clip. The portal definition is appended to
    // every scene by the item pack, so dungeon rooms can show the reference
    // return gate without inventing a rectangle or a new texture.
    if (item_assets_.IsLoaded()) {
        const soulknight::game::items::PortalDefinition* portal =
            item_assets_.FindPortal("transfer_gate_02");
        if (portal != nullptr && !portal->frames.empty()) {
            const auto marker = std::find_if(
                scene_.interaction_markers.begin(),
                scene_.interaction_markers.end(),
                [](const engine::assets::InteractionMarker& candidate) {
                    return candidate.kind == "portal";
                });
            if (marker != scene_.interaction_markers.end()) {
                const float fps = portal->fps > 0.0f ? portal->fps : 12.0f;
                const std::size_t frame_index = portal->loop
                    ? static_cast<std::size_t>(simulation_time_ * fps) %
                          portal->frames.size()
                    : std::min(portal->frames.size() - 1,
                               static_cast<std::size_t>(simulation_time_ * fps));
                const int32_t sprite_index = scene_.FindSpriteIndex(
                    portal->frames[frame_index]);
                if (sprite_index >= 0 &&
                    static_cast<std::size_t>(sprite_index) < scene_.sprites.size()) {
                    const engine::assets::SpriteData& sprite =
                        scene_.sprites[static_cast<std::size_t>(sprite_index)];
                    engine::rendering::SpriteDraw draw;
                    draw.position = marker->position;
                    draw.size = sprite.WorldSize();
                    draw.uv = sprite.UvRect();
                    draw.color = engine::Color::White();
                    draw.layer = engine::rendering::SortLayer::Character;
                    draw.sorting_order = engine::rendering::WorldDepthOrder(
                        marker->position.y, 5);
                    draw.texture_id = sprite.texture_id;
                    draw.pivot = {0.5f, 0.0f};
                    (void)sprite_batch_.DrawSprite(draw);
                }
            }
        }
    }

    soulknight::game::ui::DevOverlayContext ui_context;
    ui_context.scene = &scene_;
    ui_context.camera = &render_camera_;
    ui_context.characters = &player_characters_;
    ui_context.weapons = &weapons_;
    ui_context.enemies = &enemies_;
    ui_context.player = &player_;
    ui_context.health = &player_health_;
    ui_context.energy = &player_energy_;
    ui_context.weapon_inventory = &weapon_inventory_;
    ui_context.room_id = scene_.id.c_str();
    ui_context.room_graph = room_graph_text_.c_str();
    ui_context.facing = engine::locomotion::FacingDirectionName(facing_);
    ui_context.version = LITEKNIGHT_VERSION;
    ui_context.build = SOUL_KNIGHT_COMMIT_HASH;
    const char* video_driver = SDL_GetCurrentVideoDriver();
    ui_context.platform = video_driver != nullptr ? video_driver : "unknown";
    ui_context.renderer = "Vulkan";
    ui_context.frame_ms = static_cast<float>(frame_timer_.GetStats().frame_time_ms);
    ui_context.simulation_ticks = simulation_ticks_;
    ui_context.render_frames = render_frames_;
    ui_context.active_projectiles = projectiles_.ActiveCount();
    ui_context.pickups = weapon_pickups_.ActiveCount();
    ui_context.room_clear_pending = room_clear_pending_;
    ui_context.aim_mode = aim_mode_ == soulknight::game::player::AimMode::Auto
        ? "AUTO AIM" : "MANUAL";
    ui_context.aim_angle = AimAngleDegrees();
    ui_context.mouse_logical = last_mouse_logical_;
    ui_context.mouse_world = last_mouse_world_;
    ui_context.aim_origin = AimOrigin();
    ui_context.auto_aim = aim_mode_ == soulknight::game::player::AimMode::Auto;
    ui_context.weapon_angle = AimAngleDegrees();
    ui_context.has_auto_target = auto_aim_has_target_;
    ui_context.auto_target = auto_aim_target_;
    ui_context.muzzle_world = weapon_.IsConfigured()
        ? weapon_.MuzzlePosition(player_.Position(), aim_direction_)
        : AimOrigin();
    ui_context.map_seed = map_seed_;
    ui_context.room_count = static_cast<int>(room_manager_.Rooms().size());
    ui_context.logical_width = 1920;
    ui_context.logical_height = 1080;
    ui_context.framebuffer_width = window_.GetPixelWidth();
    ui_context.framebuffer_height = window_.GetPixelHeight();
    ui_context.framebuffer_scale = logical_viewport_.framebuffer_scale;
    ui_context.framebuffer_x = logical_viewport_.framebuffer_x;
    ui_context.framebuffer_y = logical_viewport_.framebuffer_y;
    ui_context.framebuffer_viewport_width =
        logical_viewport_.framebuffer_viewport_width;
    ui_context.framebuffer_viewport_height =
        logical_viewport_.framebuffer_viewport_height;
    ui_context.hud_visible = false;
    dev_ui_.Draw(sprite_batch_, ui_context);
    DrawGameUI();
    asset_lab_.Draw(sprite_batch_, scene_, camera_, dev_ui_);
}

void Application::DrawCollisionDebug() {
    auto& text = dev_ui_.Text();
    const auto world_rect = [&](const engine::collision::Aabb& box,
                                engine::Color color, int32_t order) {
        text.DrawPanelBorder(sprite_batch_, {box.min.x, box.max.y},
                             {box.max.x - box.min.x, box.max.y - box.min.y},
                             {0, 0, 0, 0}, color, 0.035f, order);
    };

    // Player gameplay collision footprint (circle approximated by its AABB).
    const engine::collision::Aabb player_box{
        {player_.Position().x - 0.4f, player_.Position().y + 0.1f},
        {player_.Position().x + 0.4f, player_.Position().y + 0.9f}};
    world_rect(player_box, {70, 255, 120, 255}, 910);
    text.DrawPanel(sprite_batch_, {player_.Position().x - 0.055f,
                                   player_.Position().y + 0.055f},
                   {0.11f, 0.11f}, {255, 65, 65, 255}, 911);

    // Visual sprite bounds for the active body sprite.
    if (player_characters_.Count() > 0) {
        const auto& character = player_characters_.Active();
        const int32_t sprite_index = character.body_sprite_index;
        if (sprite_index >= 0 &&
            static_cast<std::size_t>(sprite_index) < scene_.sprites.size()) {
            const auto& sprite =
                scene_.sprites[static_cast<std::size_t>(sprite_index)];
            const engine::Vec2 size = sprite.WorldSize();
            const engine::Vec2 min{
                render_player_position_.x - sprite.pivot.x * size.x,
                render_player_position_.y - sprite.pivot.y * size.y};
            text.DrawPanelBorder(
                sprite_batch_, {min.x, min.y + size.y}, size,
                {0, 0, 0, 0}, {255, 225, 80, 255}, 0.025f, 912);
            text.DrawText(sprite_batch_, "VISUAL", {min.x, min.y + size.y - 0.12f},
                          0.012f, {255, 225, 80, 255}, 913);
        }
    }

    // Obstacle collision boxes and sort anchors.
    if (!obstacles_.Variants().empty()) {
        const auto& variant = obstacles_.ActiveVariant();
        for (const auto& marker : obstacles_.Markers()) {
            const engine::collision::Aabb box{
                {marker.position.x + variant.collider.min.x,
                 marker.position.y + variant.collider.min.y},
                {marker.position.x + variant.collider.max.x,
                 marker.position.y + variant.collider.max.y}};
            world_rect(box, {255, 105, 105, 255}, 914);
            text.DrawPanel(sprite_batch_, {marker.position.x - 0.045f,
                                           marker.position.y + 0.045f},
                           {0.09f, 0.09f}, {90, 170, 255, 255}, 915);
            char label[64];
            std::snprintf(label, sizeof(label), "sort %d/%d",
                          variant.visuals.empty() ? 0
                            : variant.visuals.front().sorting_layer,
                          variant.visuals.empty() ? 0
                            : variant.visuals.front().sorting_order);
            text.DrawText(sprite_batch_, label,
                          {marker.position.x - 0.5f,
                           marker.position.y + variant.collider.max.y + 0.40f},
                          0.010f, {215, 230, 255, 255}, 916);
        }
    }
}

void Application::BuildSyntheticBatch() {
    // PROJECT DECISION / renderer bring-up only: the default texture is the
    // synthetic PNG fixture, not reference game content.
    constexpr float long_side = 8.0f;
    const float width = static_cast<float>(smoke_texture_.width);
    const float height = static_cast<float>(smoke_texture_.height);
    const float aspect = width / height;
    const engine::Vec2 size = aspect >= 1.0f
        ? engine::Vec2{long_side, long_side / aspect}
        : engine::Vec2{long_side * aspect, long_side};

    engine::rendering::SpriteDraw draw;
    draw.position = {0.0f, 0.0f};
    draw.size = size;
    draw.uv = {0.0f, 0.0f, 1.0f, 1.0f};
    draw.color = engine::Color::White();
    draw.layer = engine::rendering::SortLayer::Character;
    draw.texture_id = 0;
    draw.pivot = {0.5f, 0.5f};
    (void)sprite_batch_.DrawSprite(draw);
}

void Application::HandleSceneKeyDown(SDL_Scancode scancode,
                                   SDL_Keymod modifiers) {
    if (ui_design_mode_) return;
    if (mech_station_open_) {
        if (scancode == SDL_SCANCODE_ESCAPE) {
            mech_station_open_ = false;
            PlayEffect("ui_click", 0.3f);
        } else if (scancode == SDL_SCANCODE_UP ||
                   scancode == SDL_SCANCODE_W) {
            CycleMechSelection(-1);
        } else if (scancode == SDL_SCANCODE_DOWN ||
                   scancode == SDL_SCANCODE_S) {
            CycleMechSelection(1);
        } else if (scancode == SDL_SCANCODE_RETURN ||
                   scancode == SDL_SCANCODE_KP_ENTER ||
                   scancode == SDL_SCANCODE_E) {
            BuildMech();
        }
        return;
    }
    if (living_room_suggestions_ && scene_.is_hub &&
        scancode == SDL_SCANCODE_G) {
        suggestion_edit_mode_ = !suggestion_edit_mode_;
        suggestion_dragging_ = false;
        LOG_INFO_APP("Living Room QA mode: {}",
                     suggestion_edit_mode_ ? "ON" : "OFF");
        PlayEffect("ui_click", 0.35f);
        return;
    }
    if (interactive_test_.Active() && replay_ == nullptr) {
        interactive_test_.RecordCommand(
            simulation_ticks_ + 1, static_cast<int>(scancode),
            static_cast<int>(modifiers));
    }
    if (interactive_test_.Active() && replay_ == nullptr &&
        scancode == SDL_SCANCODE_F1) {
        FinalizeInteractiveTest("PASS", "user finalized with F1");
        running_ = false;
        return;
    }
    if (scancode == SDL_SCANCODE_O && scene_.is_hub) {
        player_orbs_ += 100;
        LOG_INFO_APP("Developer: +100 orbs (total {})", player_orbs_);
        return;
    }
    if (game_ui_.IsPaused() || game_ui_.IsMainMenu()) {
        HandleUIKey(scancode, modifiers);
        return;
    }
    if (scancode == SDL_SCANCODE_P) {
        game_ui_.OpenPause();
        return;
    }
    if (scancode == SDL_SCANCODE_T && multi_room_world_) {
        SwitchRoom();
        return;
    }
    const bool dev_tools = config_.devroom || config_.review_mode ||
                           config_.asset_lab;
    if (dev_tools && scancode == SDL_SCANCODE_N) {
        player_.SetPosition(scene_.spawn);
        previous_player_position_ = scene_.spawn;
        render_player_position_ = scene_.spawn;
        camera_.position = scene_.spawn;
        previous_camera_position_ = scene_.spawn;
        render_camera_.position = scene_.spawn;
        LOG_INFO_APP("Map tool: teleported to entrance ({:.2f},{:.2f})",
                     scene_.spawn.x, scene_.spawn.y);
        return;
    }
    if (dev_tools && scene_.is_hub && scancode == SDL_SCANCODE_H) {
        game_ui_.OpenCharacterSelect(
            static_cast<int>(player_characters_.ActiveIndex()));
        return;
    }
    if (dev_tools && scene_.is_hub && scancode == SDL_SCANCODE_B) {
        game_ui_.OpenWeaponSelect(
            static_cast<int>(weapon_inventory_.ActiveIndex()));
        return;
    }
            if (interactive_test_.Active() && replay_ == nullptr &&
                scancode == SDL_SCANCODE_RETURN &&
                !dev_ui_.MenuOpen() && !asset_lab_.IsOpen()) {
                FinalizeInteractiveTest("PASS", "user finalized with Enter");
                running_ = false;
            } else if (asset_lab_.IsOpen()) {
                if (scancode == SDL_SCANCODE_LEFTBRACKET ||
                    scancode == SDL_SCANCODE_KP_MINUS) {
                    CycleAssetLabCategory(-1);
                } else if (scancode == SDL_SCANCODE_RIGHTBRACKET ||
                           scancode == SDL_SCANCODE_KP_PLUS) {
                    CycleAssetLabCategory(1);
                } else if (scancode == SDL_SCANCODE_COMMA ||
                           scancode == SDL_SCANCODE_LEFT) {
                    CycleAssetLabItem(-1);
                } else if (scancode == SDL_SCANCODE_PERIOD ||
                           scancode == SDL_SCANCODE_RIGHT) {
                    CycleAssetLabItem(1);
                } else if (scancode == SDL_SCANCODE_SPACE) {
                    asset_lab_.TogglePause();
                } else if (scancode == SDL_SCANCODE_N ||
                           scancode == SDL_SCANCODE_F) {
                    asset_lab_.StepFrame(1);
                } else if (scancode == SDL_SCANCODE_R) {
                    asset_lab_.Restart();
                } else if (scancode == SDL_SCANCODE_RETURN) {
                    ActivateAssetLabSelection();
                } else if (scancode == SDL_SCANCODE_F5) {
                    asset_lab_.Close();
                }
            } else if (scancode == SDL_SCANCODE_F2) {
                SaveGameToDisk();
            } else if (scancode == SDL_SCANCODE_F3) {
                LoadGameFromDisk();
            } else if (scancode == SDL_SCANCODE_F4) {
                collision_debug_ = !collision_debug_;
                if (collision_debug_ && !dev_ui_.OverlayVisible()) {
                    dev_ui_.ToggleOverlay();
                }
                LOG_INFO_APP("Collision/aim debug: {}",
                             BooleanText(collision_debug_));
            } else if (scancode == SDL_SCANCODE_F5) {
                asset_lab_.Open();
                LOG_INFO_APP("Asset laboratory opened");
            } else if (scancode == SDL_SCANCODE_F10) {
                if ((modifiers & SDL_KMOD_SHIFT) != 0) {
                    player_orbs_ += 100;
                    LOG_INFO_APP("Developer: +100 orbs (total {})", player_orbs_);
                } else {
                    dev_ui_.ToggleOverlay();
                    LOG_INFO_APP("Developer overlay: {}",
                                 BooleanText(dev_ui_.OverlayVisible()));
                }
            } else if (dev_tools && scancode == SDL_SCANCODE_F9) {
                SpawnSelectedEnemy((modifiers & SDL_KMOD_SHIFT) != 0 ? 5 : 1);
            } else if (scancode == SDL_SCANCODE_F11) {
                (void)window_.SetFullscreen(!window_.IsFullscreen());
                LOG_INFO_APP("Fullscreen: {}",
                             BooleanText(window_.IsFullscreen()));
            } else if (dev_tools && scancode == SDL_SCANCODE_F12) {
                enemies_.ClearEnemies();
                LOG_INFO_APP("Developer enemy spawner: enemies cleared");
            } else if (dev_ui_.MenuOpen()) {
                const auto menu = dev_ui_.CurrentMenu();
                if (scancode == SDL_SCANCODE_F6) {
                    dev_ui_.OpenMenu(soulknight::game::ui::DevMenu::Characters,
                                     player_characters_.ActiveIndex(),
                                     player_characters_.Count());
                } else if (scancode == SDL_SCANCODE_F7) {
                    dev_ui_.OpenMenu(soulknight::game::ui::DevMenu::Weapons,
                                     weapons_.ActiveIndex(), weapons_.Count());
                } else if (scancode == SDL_SCANCODE_F8) {
                    dev_ui_.OpenMenu(soulknight::game::ui::DevMenu::Enemies,
                                     dev_ui_.SelectedIndex(),
                                     enemies_.DefinitionCount());
                } else if (scancode == SDL_SCANCODE_A ||
                    scancode == SDL_SCANCODE_LEFT ||
                    scancode == SDL_SCANCODE_W ||
                    scancode == SDL_SCANCODE_UP) {
                    dev_ui_.Navigate(-1);
                } else if (scancode == SDL_SCANCODE_D ||
                           scancode == SDL_SCANCODE_RIGHT ||
                           scancode == SDL_SCANCODE_S ||
                           scancode == SDL_SCANCODE_DOWN) {
                    dev_ui_.Navigate(1);
                } else if (scancode == SDL_SCANCODE_RETURN) {
                    if (menu == soulknight::game::ui::DevMenu::Characters) {
                        SwitchCharacter(dev_ui_.SelectedIndex());
                    } else if (menu == soulknight::game::ui::DevMenu::Weapons) {
                        if (weapon_inventory_.EquipActive(dev_ui_.SelectedIndex())) {
                            ConfigureActiveWeapon();
                            ConfigureSecondaryWeapon();
                        }
                    }
                    dev_ui_.CloseMenu();
                }
            } else if (scancode == SDL_SCANCODE_F6) {
                dev_ui_.OpenMenu(soulknight::game::ui::DevMenu::Characters,
                                 player_characters_.ActiveIndex(),
                                 player_characters_.Count());
            } else if (scancode == SDL_SCANCODE_F7) {
                dev_ui_.OpenMenu(soulknight::game::ui::DevMenu::Weapons,
                                 weapons_.ActiveIndex(), weapons_.Count());
            } else if (scancode == SDL_SCANCODE_F8) {
                dev_ui_.OpenMenu(soulknight::game::ui::DevMenu::Enemies,
                                 dev_ui_.SelectedIndex(),
                                 enemies_.DefinitionCount());
            } else if (scancode == SDL_SCANCODE_E) {
                if (!TryOpenChest()) {
                    if (!TryPickupWeapon()) TryInteract();
                }
            } else if (dev_tools && scancode == SDL_SCANCODE_G) {
                SpawnWeaponPickup(weapons_.ActiveIndex());
            } else if (scancode == SDL_SCANCODE_O &&
                       (config_.devroom || config_.review_mode)) {
                // Developer-only control; normal gameplay never requires it.
                if (!scene_.doors.empty()) {
                    ToggleAllDoors();
                } else {
                    player_orbs_ += 100;
                    LOG_INFO_APP("Developer: +100 orbs (total {})",
                                 player_orbs_);
                }
            } else if (dev_tools && scancode == SDL_SCANCODE_C &&
                       !scene_.doors.empty()) {
                CloseAllDoors();
            } else if (dev_tools && scancode == SDL_SCANCODE_LEFTBRACKET) {
                CycleCharacter(-1);
            } else if (dev_tools && scancode == SDL_SCANCODE_RIGHTBRACKET) {
                CycleCharacter(1);
            } else if (dev_tools && scancode == SDL_SCANCODE_Y) {
                CycleObstacleVariant();
            } else if (dev_tools && scancode == SDL_SCANCODE_R) {
                ResetCombat();
            } else if (dev_tools && (modifiers & SDL_KMOD_CTRL) != 0 &&
                       scancode >= SDL_SCANCODE_1 &&
                       scancode <= SDL_SCANCODE_9) {
                SwitchCharacter(static_cast<std::size_t>(
                    scancode - SDL_SCANCODE_1));
            } else if (dev_tools && (modifiers & SDL_KMOD_CTRL) != 0 &&
                       scancode == SDL_SCANCODE_0) {
                SwitchCharacter(9);
            } else if (scancode == SDL_SCANCODE_D &&
                       game_ui_.Screen() ==
                           soulknight::game::ui::UIScreen::Workshop) {
                const std::size_t slot = weapon_inventory_.ActiveSlot();
                if (weapon_inventory_.SlotOccupied(slot)) {
                    const std::size_t index = weapon_inventory_.Slot(slot);
                    if (index < weapons_.Count()) {
                        const std::string weapon_id =
                            weapons_.Definitions()[index].id;
                        if (workshop_.AddMaterialsForDismantle(weapon_id) !=
                            nullptr) {
                            (void)weapon_inventory_.ClearActive();
                            ConfigureActiveWeapon();
                            ConfigureSecondaryWeapon();
                            workshop_status_ =
                                "DISMANTLED " + weapon_id;
                            PlayEffect("pickup");
                            LOG_INFO_APP("Workshop: dismantled {}", weapon_id);
                            SaveGameToDisk();
                        } else {
                            workshop_status_ = "NO RECOVERED RECIPE";
                        }
                    }
                }
                return;
            } else if (scancode == SDL_SCANCODE_Q) {
                ActivateKnightPower();
            } else if (scancode == SDL_SCANCODE_TAB) {
                const std::size_t other =
                    weapon_inventory_.ActiveSlot() == 0 ? 1 : 0;
                if (weapon_inventory_.SlotOccupied(other)) {
                    SwitchWeaponSlot(other);
                }
            } else if (scancode == SDL_SCANCODE_1) {
                SwitchWeaponSlot(0);
            } else if (scancode == SDL_SCANCODE_2) {
                SwitchWeaponSlot(1);
            }
}

void Application::HandleUIKey(SDL_Scancode scancode,
                              SDL_Keymod /*modifiers*/) {
    using soulknight::game::ui::UIScreen;
    if (scancode == SDL_SCANCODE_UP || scancode == SDL_SCANCODE_W) {
        game_ui_.Navigate(-1);
    } else if (scancode == SDL_SCANCODE_DOWN ||
               scancode == SDL_SCANCODE_S) {
        game_ui_.Navigate(1);
    } else if (scancode == SDL_SCANCODE_LEFT || scancode == SDL_SCANCODE_A) {
        game_ui_.Navigate(-1);
    } else if (scancode == SDL_SCANCODE_RIGHT ||
               scancode == SDL_SCANCODE_D) {
        game_ui_.Navigate(1);
    } else if (scancode == SDL_SCANCODE_RETURN ||
               scancode == SDL_SCANCODE_KP_ENTER ||
               scancode == SDL_SCANCODE_SPACE) {
        game_ui_.Confirm();
        ProcessUIAction();
    } else if (scancode == SDL_SCANCODE_ESCAPE) {
        // The global Escape hierarchy owns pause/settings transitions. Do not
        // also resume here, or a single Escape would pause and unpause.
        if (game_ui_.Screen() == UIScreen::Settings) {
            game_ui_.Back();
        }
    } else if (scancode == SDL_SCANCODE_P && game_ui_.IsPaused()) {
        game_ui_.Resume();
    }
}

void Application::ProcessUIAction() {
    using soulknight::game::ui::UIAction;
    const UIAction action = game_ui_.TakeAction();
    if (action != UIAction::None) PlayUiSound("ui_click");
    switch (action) {
        case UIAction::None:
            break;
        case UIAction::Resume:
            game_ui_.Resume();
            break;
        case UIAction::Home:
            if (scene_mode_) SaveGameToDisk();
            game_ui_.OpenMainMenu();
            break;
        case UIAction::NewGame: {
            // New Game always begins at the first recovered MapManagerLevel
            // (1-1), never at a random biome or the previous run's level.
            biome_.Reset();
            map_seed_ = config_.map_seed;
            cleared_rooms_.clear();
            chest_spawned_rooms_.clear();
            room_enemy_snapshots_.clear();
            visited_rooms_.clear();
            sold_shop_items_.clear();
            active_statue_id_.clear();
            active_mercenary_id_.clear();
            active_mount_id_.clear();
            mount_speed_bonus_ = 0.0f;
            mercenary_level_ = 1;
            player_coins_ = 0;
            player_orbs_ = 0;
            if (!config_.map_seed_set) {
                std::random_device device;
                trader_stock_seed_ = static_cast<std::int64_t>(
                    (static_cast<std::uint64_t>(device()) << 32) ^
                    static_cast<std::uint64_t>(device()));
            } else {
                trader_stock_seed_ = static_cast<std::int64_t>(map_seed_) ^
                    0x54524144;
            }
            ConfigureTraderOffers();
            plant_system_.Clear();
            selected_plant_seed_.clear();
            if (multi_room_world_) {
                if (scene_.id != world_hub_id_) {
                    (void)ActivateRoom(world_hub_id_, "");
                }
                stored_rooms_.erase(
                    std::remove_if(stored_rooms_.begin(), stored_rooms_.end(),
                                   [&](const StoredRoom& room) {
                                       return room.id != world_hub_id_;
                                   }),
                    stored_rooms_.end());
                multi_room_world_ = false;
                (void)LoadWorldRooms();
            }
            ResetCombat();
            if (scene_.is_hub) ConfigureHubGarden();
            game_ui_.OpenGameplay();
            LOG_INFO_APP("New Game: level={} seed={} scene={}",
                         biome_.Current() != nullptr
                             ? biome_.Current()->id : "none",
                         map_seed_, scene_.id);
            break;
        }
        case UIAction::ContinueGame:
            if (save_manager_ != nullptr && save_manager_->Exists()) {
                LoadGameFromDisk();
                game_ui_.OpenGameplay();
            } else {
                LOG_WARN_APP("Continue: no native save slot exists");
            }
            break;
        case UIAction::Settings:
            game_ui_.OpenSettings();
            break;
        case UIAction::Back:
            game_ui_.Back();
            break;
        case UIAction::Quit:
            running_ = false;
            break;
        case UIAction::ToggleFullscreen:
            (void)window_.SetFullscreen(!window_.IsFullscreen());
            break;
        case UIAction::ToggleDebugOverlay:
            dev_ui_.ToggleOverlay();
            break;
        case UIAction::SelectCharacter:
            SwitchCharacter(static_cast<std::size_t>(
                std::max(0, game_ui_.CharacterSelection())));
            game_ui_.CloseSelection();
            break;
        case UIAction::SelectWeapon:
            if (weapon_inventory_.EquipActive(static_cast<std::size_t>(
                    std::max(0, game_ui_.WeaponSelection())))) {
                ConfigureActiveWeapon();
                ConfigureSecondaryWeapon();
            }
            game_ui_.CloseSelection();
            break;
        case UIAction::CraftWeapon: {
            const int selected = game_ui_.WorkshopSelection();
            if (selected < 0 ||
                static_cast<std::size_t>(selected) >= workshop_.Recipes().size()) {
                break;
            }
            const auto& recipe = workshop_.Recipes()[
                static_cast<std::size_t>(selected)];
            std::size_t weapon_index = weapons_.Count();
            for (std::size_t i = 0; i < weapons_.Count(); ++i) {
                if (weapons_.Definitions()[i].id == recipe.weapon_id) {
                    weapon_index = i;
                    break;
                }
            }
            if (weapon_index >= weapons_.Count()) {
                workshop_status_ = "NOT AVAILABLE IN THIS ROOM";
                break;
            }
            if (workshop_.RequiresBlueprint(recipe.weapon_id) &&
                !workshop_.HasBlueprint(recipe.weapon_id)) {
                workshop_status_ = "NEED TO COLLECT WEAPON BLUEPRINT FIRST";
                PlayUiSound("ui_click");
                break;
            }
            if (!workshop_.HasMaterials(recipe.weapon_id)) {
                workshop_status_ = "MISSING MATERIALS";
                PlayUiSound("ui_click");
                break;
            }
            if (workshop_.ConsumeForCraft(recipe.weapon_id)) {
                const std::size_t slot = weapon_inventory_.ActiveSlot();
                if (weapon_inventory_.SlotOccupied(slot)) {
                    const std::size_t old = weapon_inventory_.Slot(slot);
                    if (old < weapons_.Count()) {
                        (void)weapon_pickups_.Spawn(
                            old, player_.Position() + engine::Vec2{1.2f, 0.0f});
                    }
                }
                (void)weapon_inventory_.EquipActive(weapon_index);
                ConfigureActiveWeapon();
                ConfigureSecondaryWeapon();
                workshop_status_ = "CRAFTED " + recipe.name;
                PlayEffect("pickup");
                LOG_INFO_APP("Workshop: crafted {}", recipe.weapon_id);
                SaveGameToDisk();
            }
            break;
        }
        case UIAction::ToggleReducedWeaponEffects:
            reduced_weapon_effects_ = !reduced_weapon_effects_;
            LOG_INFO_APP("Weapon effects: {}",
                         reduced_weapon_effects_ ? "SUBTLE" : "FULL");
            if (scene_mode_) SaveGameToDisk();
            break;
        case UIAction::ToggleLivingRoomSuggestions:
            if (!scene_.is_hub) {
                ShowDialogue("", "LIVING ROOM ONLY", 1.5f);
                break;
            }
            living_room_suggestions_ = !living_room_suggestions_;
            LOG_INFO_APP("Living Room QA suggestions: {}",
                         BooleanText(living_room_suggestions_));
            if (scene_mode_) SaveGameToDisk();
            break;
        case UIAction::ToggleAimMode:
            aim_mode_ = aim_mode_ == soulknight::game::player::AimMode::Manual
                ? soulknight::game::player::AimMode::Auto
                : soulknight::game::player::AimMode::Manual;
            mouse_aim_valid_ = true;
            LOG_INFO_APP("Aiming mode: {}", aim_mode_ ==
                soulknight::game::player::AimMode::Auto ? "AUTO AIM" : "MANUAL");
            if (scene_mode_) SaveGameToDisk();
            break;
        case UIAction::OpenUiDesignMode:
            EnterUiDesignMode();
            break;
        case UIAction::OpenDevroom:
            if (multi_room_world_ && scene_.is_hub) {
                (void)SwitchRoom("hub_door");
            }
            game_ui_.OpenGameplay();
            break;
        case UIAction::OpenSaveLoad:
            game_ui_.OpenSaveLoad();
            break;
        case UIAction::SaveSlot:
            if (scene_mode_) SaveGameToDisk();
            RefreshSaveSummary();
            break;
        case UIAction::LoadSlot:
            if (save_manager_ != nullptr && save_manager_->Exists()) {
                LoadGameFromDisk();
                game_ui_.OpenGameplay();
            }
            break;
        case UIAction::BuyTraderItem:
            BuyTraderItem();
            break;
        case UIAction::SelectPet:
            if (pet_.Select(static_cast<std::size_t>(
                    std::max(0, game_ui_.PetSelection())))) {
                active_pet_id_ = pet_.SelectedId();
                PlayEffect("pickup", 0.6f);
                LOG_INFO_APP("Pet selected: {} ({})", pet_.Selected()->display_name,
                             active_pet_id_);
                if (scene_mode_) SaveGameToDisk();
            }
            game_ui_.CloseSelection();
            break;
        case UIAction::DeleteSlot:
            if (save_manager_ != nullptr) {
                std::string error;
                if (save_manager_->Delete(&error)) {
                    LOG_INFO_APP("Native save deleted");
                } else {
                    LOG_ERROR_APP("Save delete failed: {}", error);
                }
                RefreshSaveSummary();
            }
            break;
        case UIAction::ResetSave:
            if (save_manager_ != nullptr) {
                std::string error;
                if (save_manager_->Delete(&error)) {
                    LOG_INFO_APP("Native save reset");
                } else {
                    LOG_ERROR_APP("Save reset failed: {}", error);
                }
                RefreshSaveSummary();
            }
            break;
    }
}

void Application::LoadUiLayout() {
    const std::filesystem::path path =
        filesystem_.GetConfigHome() / "liteknight" / "ui_layout.json";
    std::string error;
    if (hud_layout_.Load(path, &error)) {
        LOG_INFO_APP("UI layout loaded: {}", path.string());
    } else if (!error.empty()) {
        LOG_WARN_APP("UI layout load failed ({}); using default layout", error);
    } else {
        LOG_INFO_APP("UI layout default: {}", path.string());
    }
}

void Application::SaveUiLayout() {
    const std::filesystem::path path =
        filesystem_.GetConfigHome() / "liteknight" / "ui_layout.json";
    std::string error;
    if (hud_layout_.Save(path, &error)) {
        LOG_INFO_APP("UI layout saved: {}", path.string());
    } else {
        LOG_ERROR_APP("UI layout save failed: {}", error);
    }
}

void Application::EnterUiDesignMode() {
    if (!scene_mode_) return;
    ui_design_from_main_menu_ =
        game_ui_.SettingsParent() == soulknight::game::ui::UIScreen::MainMenu;
    ui_design_mode_ = true;
    ui_design_dragging_ = false;
    ui_design_selected_.clear();
    if (ui_design_from_main_menu_) {
        game_ui_.OpenMainMenu();
    } else {
        game_ui_.OpenGameplay();
    }
    movement_input_ = {};
    attack_held_ = false;
    LOG_INFO_APP("UI Design Mode: drag {} elements; ESC saves and exits",
                 ui_design_from_main_menu_ ? "menu" : "HUD");
}

void Application::ExitUiDesignMode() {
    if (!ui_design_mode_) return;
    ui_design_mode_ = false;
    ui_design_dragging_ = false;
    SaveUiLayout();
    if (ui_design_from_main_menu_) {
        game_ui_.OpenMainMenu();
    } else {
        game_ui_.OpenSettings();
    }
    ui_design_from_main_menu_ = false;
    LOG_INFO_APP("UI Design Mode exited");
}

bool Application::HandleUiDesignEvent(const SDL_Event& event) {
    if (!ui_design_mode_) return false;
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
            ExitUiDesignMode();
            return true;
        }
        if (event.key.scancode == SDL_SCANCODE_R) {
            hud_layout_.ResetToDefaults();
            ui_design_selected_.clear();
            LOG_INFO_APP("UI layout reset to default");
            return true;
        }
        if (event.key.scancode == SDL_SCANCODE_S) {
            SaveUiLayout();
            return true;
        }
        return false;
    }
    if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event.type != SDL_EVENT_MOUSE_BUTTON_UP &&
        event.type != SDL_EVENT_MOUSE_MOTION) {
        return false;
    }
    const engine::Vec2 logical = logical_viewport_.WindowPointToLogical(
        {event.type == SDL_EVENT_MOUSE_MOTION
             ? event.motion.x : event.button.x,
         event.type == SDL_EVENT_MOUSE_MOTION
             ? event.motion.y : event.button.y});
    ui_design_mouse_ = logical;
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event.button.button == SDL_BUTTON_LEFT) {
        LOG_INFO_APP("UI Design pointer down at logical ({:.1f},{:.1f})",
                     logical.x, logical.y);
        if (const auto* hit = hud_layout_.HitTest(logical); hit != nullptr) {
            ui_design_selected_ = hit->id;
            ui_design_dragging_ = true;
            ui_design_drag_offset_ = logical - engine::Vec2{hit->rect.x,
                                                            hit->rect.y};
            LOG_INFO_APP("UI Design: selected {} at ({:.1f},{:.1f})",
                         hit->id, hit->rect.x, hit->rect.y);
        }
        return true;
    }
    if (event.type == SDL_EVENT_MOUSE_MOTION && ui_design_dragging_ &&
        !ui_design_selected_.empty()) {
        (void)hud_layout_.MoveTo(
            ui_design_selected_,
            {logical.x - ui_design_drag_offset_.x,
             logical.y - ui_design_drag_offset_.y});
        return true;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
        event.button.button == SDL_BUTTON_LEFT) {
        if (ui_design_dragging_) SaveUiLayout();
        ui_design_dragging_ = false;
        return true;
    }
    return false;
}

void Application::HandleMainMenuPointer(const SDL_Event& event) {
    if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event.button.button != SDL_BUTTON_LEFT ||
        !game_ui_.IsMainMenu() || ui_design_mode_) {
        return;
    }
    const engine::Vec2 logical = logical_viewport_.WindowPointToLogical(
        {event.button.x, event.button.y});
    if (game_ui_.HandlePointerClick(logical, &hud_layout_)) ProcessUIAction();
}

void Application::TeleportForReview(const engine::Vec2& position) {
    player_.SetPosition(position);
    previous_player_position_ = position;
    render_player_position_ = position;
    camera_.position = position;
    if (camera_bounds_valid_) {
        camera_.ClampToRect(camera_bounds_.min, camera_bounds_.max);
    }
    previous_camera_position_ = camera_.position;
    render_camera_ = camera_;
    LOG_INFO_APP("Review teleport: player=({:.2f},{:.2f}) camera=({:.2f},{:.2f})",
                 position.x, position.y, camera_.position.x,
                 camera_.position.y);
}

void Application::ConfigureReviewMode() {
    review_mode_ = config_.review_mode;
    if (!review_mode_) return;
    LOG_INFO_APP("Launch area: {}", config_.review_area);
    if (config_.review_area == "settings") {
        game_ui_.OpenMainMenu();
        game_ui_.OpenSettings();
        LOG_INFO_APP("Review mode: settings screen={}",
                     static_cast<int>(game_ui_.Screen()));
    } else if (config_.review_area == "living_room" ||
               config_.review_area.rfind("r1_", 0) == 0 ||
               config_.review_area == "devroom" ||
               config_.review_area == "chest" ||
               config_.review_area == "r_sell" ||
               config_.review_area == "r_statue" ||
               config_.review_area == "r_mineral" ||
               config_.review_area == "r_mercenary" ||
               config_.review_area == "r_mount") {
        game_ui_.OpenGameplay();
    } else if (config_.review_area == "workshop" ||
               config_.review_area == "garden" ||
               config_.review_area == "cellar" ||
               config_.review_area == "trader" ||
               config_.review_area == "trader-world" ||
               config_.review_area == "mech" ||
               config_.review_area == "reforging" ||
               config_.review_area == "planting" ||
               config_.review_area == "mech-ui" ||
               config_.review_area == "suggestions") {
        game_ui_.OpenGameplay();
        engine::Vec2 target = scene_.spawn;
        if (config_.review_area == "cellar") {
            target = {-10.5f, -10.0f};
        } else if (config_.review_area == "trader" ||
                   config_.review_area == "trader-world") {
            target = {27.6f, 2.2f};
        } else if (config_.review_area == "mech" ||
                   config_.review_area == "mech-ui") {
            target = {43.0f, 3.4f};
        } else if (config_.review_area == "reforging") {
            target = {35.9f, -2.2f};
        } else if (config_.review_area == "planting") {
            target = {-25.77f, -0.4f};
        }
        const char* marker_kind = config_.review_area == "workshop"
            ? "forge"
            : config_.review_area == "garden" ||
                      config_.review_area == "planting"
            ? "garden"
            : config_.review_area == "mech"
            ? "mech"
            : config_.review_area == "reforging"
            ? "forge"
            : config_.review_area == "cellar" ? "cellar" : nullptr;
        if (marker_kind != nullptr) {
            for (const auto& marker : scene_.interaction_markers) {
                if (marker.kind != marker_kind) continue;
                target = marker.position + engine::Vec2{0.0f, -2.2f};
                if (config_.review_area == "planting") target.y = -0.4f;
                if (config_.review_area == "mech" ||
                    config_.review_area == "mech-ui") {
                    target.y = 3.4f;
                }
                if (config_.review_area == "reforging" ||
                    config_.review_area == "workshop") {
                    target.y = -1.9f;
                }
                break;
            }
        }
        TeleportForReview(target);
        if (config_.review_area == "trader") {
            game_ui_.OpenTrader();
        } else if (config_.review_area == "mech-ui") {
            OpenMechStation();
        } else if (config_.review_area == "suggestions") {
            living_room_suggestions_ = true;
            suggestion_edit_mode_ = true;
            suggestion_list_visible_ = true;
            TeleportForReview({-26.0f, -0.5f});
        } else if (config_.review_area == "planting") {
            // QA-only inspection state: plant one recovered carrot so the
            // generated prefab stages can be visually checked. This never
            // runs in normal gameplay.
            if (!plant_system_.Plots().empty()) {
                plant_system_.AddSeed("plant_carrot", 1);
                const auto* plant = item_assets_.FindPlant("plant_carrot");
                (void)plant_system_.Interact(
                    0, "plant_carrot",
                    plant != nullptr ? plant->growth_seconds : 30.0f,
                    plant != nullptr
                        ? static_cast<int>(plant->states.size()) : 2,
                    plant != nullptr ? plant->item_id : std::string{},
                    plant != nullptr ? plant->harvest_count : 1);
            }
        }
    } else if (config_.review_area == "sign1" ||
               config_.review_area == "mission-board" ||
               config_.review_area == "weapon-chest" ||
               config_.review_area == "table-characters" ||
               config_.review_area == "stair" ||
               config_.review_area == "portal" ||
               config_.review_area == "sign2" ||
               config_.review_area == "table17" ||
               config_.review_area == "object27" ||
               config_.review_area == "npc-grave" ||
               config_.review_area == "table31" ||
               config_.review_area == "table35" ||
               config_.review_area == "table39" ||
               config_.review_area == "rogue" ||
               config_.review_area == "sign3" ||
               config_.review_area == "wall-short" ||
               config_.review_area == "invisible-wall" ||
               config_.review_area == "oxygen" ||
               config_.review_area == "material-chest-ui" ||
               config_.review_area == "weapon-station-ui" ||
               config_.review_area == "weapon-station-locked" ||
               config_.review_area == "trader-ui" ||
               config_.review_area == "mech-ui-live") {
        // Deterministic visual-inspection teleports for every supplied bug
        // coordinate. The player is placed south of the subject so the
        // camera centers the reported object.
        game_ui_.OpenGameplay();
        engine::Vec2 target = scene_.spawn;
        const std::string& area = config_.review_area;
        if (area == "sign1") target = {-12.63f, 3.9f};
        else if (area == "mission-board") target = {-8.1f, 3.0f};
        else if (area == "weapon-chest") target = {-2.64f, 3.2f};
        else if (area == "table-characters") target = {-0.28f, -1.2f};
        else if (area == "stair") target = {-10.4f, -6.9f};
        else if (area == "portal") target = {13.72f, -12.6f};
        else if (area == "sign2") target = {21.03f, -19.4f};
        else if (area == "table17") target = {17.4f, -24.0f};
        else if (area == "object27") target = {27.6f, -24.0f};
        else if (area == "npc-grave") target = {31.35f, -18.0f};
        else if (area == "table31") target = {31.01f, -1.4f};
        else if (area == "table35") target = {35.04f, -1.4f};
        else if (area == "table39") target = {39.72f, -1.4f};
        else if (area == "rogue") target = {10.45f, -0.2f};
        else if (area == "sign3") target = {13.79f, 3.4f};
        else if (area == "wall-short") target = {6.6f, -21.0f};
        else if (area == "invisible-wall") target = {20.08f, -8.9f};
        else if (area == "oxygen") target = {23.62f, -2.6f};
        else if (area == "material-chest-ui") target = {48.93f, 2.8f};
        else if (area == "weapon-station-ui" ||
                 area == "weapon-station-locked") {
            target = {44.98f, -4.1f};
        } else if (area == "trader-ui") {
            target = {27.6f, 2.2f};
        } else if (area == "mech-ui-live") {
            target = {43.0f, 3.4f};
        }
        TeleportForReview(target);
        if (area == "material-chest-ui") {
            // QA-only stock so the recovered material icons/name/quantity
            // layout is visibly inspectable without a pre-populated save.
            workshop_.AddMaterial("material_iron", 37);
            workshop_.AddMaterial("material_wood", 112);
            workshop_.AddMaterial("material_gem", 8);
            workshop_.AddMaterial("material_gear", 12);
            game_ui_.OpenMaterialChest();
        } else if (area == "weapon-station-ui") {
            workshop_status_ = "WEAPON STATION - BLUEPRINTS AND MATERIALS";
            game_ui_.OpenWorkshop(
                0, static_cast<int>(workshop_.Recipes().size()));
        } else if (area == "weapon-station-locked") {
            workshop_status_ = "WEAPON STATION - BLUEPRINTS AND MATERIALS";
            int locked_index = 0;
            for (std::size_t i = 0; i < workshop_.Recipes().size(); ++i) {
                if (workshop_.RequiresBlueprint(
                        workshop_.Recipes()[i].weapon_id)) {
                    locked_index = static_cast<int>(i);
                    break;
                }
            }
            game_ui_.OpenWorkshop(
                locked_index, static_cast<int>(workshop_.Recipes().size()));
        } else if (area == "trader-ui") {
            game_ui_.OpenTrader();
        } else if (area == "mech-ui-live") {
            OpenMechStation();
        }
    } else if (config_.review_area == "calibration") {
        game_ui_.OpenGameplay();
        calibration_mode_ = true;
        TeleportForReview({-40.0f, 0.0f});
        LOG_INFO_APP("Calibration: player={} collider={:.2f}x{:.2f} "
                     "camera_ortho={:.2f} view={:.2f}x{:.2f}",
                     player_characters_.Active().id,
                     player_.Config().collider_half_extents.x * 2.0f,
                     player_.Config().collider_half_extents.y * 2.0f,
                     camera_.orthographic_size, camera_.HalfWidth() * 2.0f,
                     camera_.HalfHeight() * 2.0f);
    } else if (config_.review_area == "weapons") {
        game_ui_.OpenGameplay();
        game_ui_.OpenWeaponSelect(0);
    } else if (config_.review_area == "pause") {
        game_ui_.OpenGameplay();
        game_ui_.OpenPause();
    } else if (config_.review_area == "ui_design") {
        game_ui_.OpenGameplay();
        ui_design_mode_ = true;
    } else if (config_.review_area == "fullscreen") {
        game_ui_.OpenMainMenu();
    }
}

void Application::LogViewportContract() {
    if (logical_viewport_.framebuffer_width == last_framebuffer_width_ &&
        logical_viewport_.framebuffer_height == last_framebuffer_height_ &&
        logical_viewport_.window_width == last_window_width_ &&
        logical_viewport_.window_height == last_window_height_) {
        return;
    }
    last_framebuffer_width_ = logical_viewport_.framebuffer_width;
    last_framebuffer_height_ = logical_viewport_.framebuffer_height;
    last_window_width_ = logical_viewport_.window_width;
    last_window_height_ = logical_viewport_.window_height;
    const int left = logical_viewport_.framebuffer_x;
    const int top = logical_viewport_.framebuffer_y;
    const int right = logical_viewport_.framebuffer_width -
        (left + logical_viewport_.framebuffer_viewport_width);
    const int bottom = logical_viewport_.framebuffer_height -
        (top + logical_viewport_.framebuffer_viewport_height);
    const engine::Vec2 center = logical_viewport_.WindowPointToLogical(
        {static_cast<float>(logical_viewport_.window_width) * 0.5f,
         static_cast<float>(logical_viewport_.window_height) * 0.5f});
    LOG_INFO_APP("Viewport contract: drawable {}x{}, viewport [{}x{} at {},{}] "
                 "{}x{}, scale {:.4f}x, borders L{} R{} T{} B{}, window {}x{}",
                 logical_viewport_.framebuffer_width,
                 logical_viewport_.framebuffer_height,
                 logical_viewport_.framebuffer_viewport_width,
                 logical_viewport_.framebuffer_viewport_height,
                 left, top, logical_viewport_.window_width,
                 logical_viewport_.window_height,
                 logical_viewport_.framebuffer_scale, left, right, top, bottom,
                 logical_viewport_.window_width, logical_viewport_.window_height);
    LOG_INFO_APP("Viewport mouse mapping: window center -> logical ({:.2f},{:.2f})",
                 center.x, center.y);
}

void Application::DrawGameUI() {
    if (!scene_mode_ || asset_lab_.IsOpen()) return;
    soulknight::game::ui::GameUIData data;
    data.scene = &scene_;
    data.characters = &player_characters_;
    data.weapons = &weapons_;
    data.inventory = &weapon_inventory_;
    data.health = &player_health_;
    data.energy = &player_energy_;
    data.paused = game_ui_.IsPaused();
    data.dead = player_dead_;
    data.save_exists = save_manager_ != nullptr && save_manager_->Exists();
    data.save_slot_summary = save_slot_summary_;
    data.fullscreen = window_.IsFullscreen();
    data.debug_overlay = dev_ui_.OverlayVisible();
    data.has_pixel_font = dev_ui_.HasPixelFont();
    data.auto_aim = aim_mode_ == soulknight::game::player::AimMode::Auto;
    data.reduced_weapon_effects = reduced_weapon_effects_;
    data.living_room_suggestions = living_room_suggestions_;
    data.is_hub = scene_.is_hub;
    if (abilities_.Count() > 0) {
        const auto& ability = abilities_.Abilities()[0];
        data.ability_state = static_cast<int>(ability.state);
        if (ability.state == soulknight::game::player::AbilityState::Cooldown &&
            ability.definition.cooldown > 0.0f) {
            data.ability_cooldown = ability.timer / ability.definition.cooldown;
        }
    }
    data.ui_assets = ui_assets_.IsLoaded() ? &ui_assets_ : nullptr;
    data.layout = &hud_layout_;
    data.ui_design_mode = ui_design_mode_;
    data.ui_design_selected = ui_design_selected_;
    data.design_mouse = ui_design_mouse_;
    data.coins = player_coins_;
    data.orbs = player_orbs_;
    data.show_orbs = scene_.is_hub;
    data.trader_offers = trader_offers_;
    data.trader_selected = game_ui_.TraderSelection();
    data.map_rooms.clear();
    data.map_links.clear();
    for (const auto& room : room_manager_.Rooms()) {
        if (room.id == world_hub_id_) continue;
        // UIMap.GetMinimapSprite: room_type 0 -> home_logo (levelIndex <= 15),
        // room_type 3 -> chest/special logo, other room types have no icon.
        std::string icon;
        switch (room.type) {
            case 0: icon = "home"; break;
            case 3: icon = "chest"; break;
            case 4: icon = "special"; break;
            default: break;
        }
        data.map_rooms.push_back(soulknight::game::ui::MapRoomView{
            room.id, room.map_x, room.map_y, room.id == scene_.id,
            std::find(visited_rooms_.begin(), visited_rooms_.end(), room.id) !=
                visited_rooms_.end(),
            false, icon});
    }
    data.devroom_room_id.clear();
    data.room_label = scene_.room.label;
    if (!scene_.is_hub) {
        const auto stage = room_stage_labels_.find(scene_.id);
        if (stage != room_stage_labels_.end()) {
            data.room_label = stage->second;
        }
    }
    data.room_clear_label = scene_.room.clear_label;
    data.room_cleared = scene_.room.type == 1 &&
        cleared_rooms_.contains(scene_.id);
    data.room_title_timer = room_title_timer_;
    data.room_clear_timer = room_clear_timer_;
    data.transition_flash = room_transition_flash_;
    for (const auto& connection : room_manager_.Connections()) {
        if (connection.from == world_hub_id_ ||
            connection.to == world_hub_id_) {
            continue;
        }
        const auto* from = room_manager_.Find(connection.from);
        const auto* to = room_manager_.Find(connection.to);
        if (from == nullptr || to == nullptr) continue;
        data.map_links.push_back(soulknight::game::ui::MapLinkView{
            from->map_x, from->map_y, to->map_x, to->map_y});
    }
    if (const auto* chest = item_world_.NearestClosedChest(
            player_.Position(), 1.8f); chest != nullptr) {
        data.chest_nearby = true;
        data.chest_label = "CHEST";
    } else if (const auto* pickup = weapon_pickups_.Nearest(player_.Position(), 1.5f);
        pickup != nullptr && pickup->weapon_index < weapons_.Count()) {
        data.pickup_nearby = true;
        const auto& pickup_weapon = weapons_.Definitions()[pickup->weapon_index];
        data.pickup_name = pickup_weapon.display_name.empty()
            ? pickup_weapon.id : pickup_weapon.display_name;
        data.pickup_sprite_index = pickup_weapon.visual_sprite_index;
        data.pickup_damage = pickup_weapon.damage;
        data.pickup_energy_cost = pickup_weapon.EnergyCost();
        data.pickup_cycle_seconds = pickup_weapon.attack_cycle_seconds;
        data.pickup_screen = render_camera_.WorldToScreen(pickup->position);
    } else if (const auto* marker = NearbyInteraction(); marker != nullptr) {
        data.interaction_nearby = true;
        data.interaction_label = marker->label.empty() ? marker->id
                                                       : marker->label;
        if (marker->kind == "portal" &&
            return_portal_confirm_seconds_ >= 0.0 &&
            NowSeconds() - return_portal_confirm_seconds_ <= 3.0) {
            data.interaction_label = "NEXT LEVEL? E AGAIN";
        }
        data.interaction_screen = render_camera_.WorldToScreen(marker->position);
    }
    if (mech_station_open_ || suggestion_edit_mode_ || calibration_mode_) {
        data.chest_nearby = false;
        data.pickup_nearby = false;
        data.interaction_nearby = false;
    }
    data.dialogue_speaker = dialogue_speaker_;
    data.dialogue_text = dialogue_text_;
    data.dialogue_timer = dialogue_timer_;
    data.stake_damage = stake_damage_;
    data.stake_damage_screen = stake_damage_screen_;
    data.stake_damage_timer = stake_damage_timer_;
    data.material_rows.clear();
    for (const auto& material : workshop_.SnapshotMaterials()) {
        soulknight::game::ui::MaterialRowView row;
        row.name = workshop_.MaterialName(material.id);
        row.count = material.count;
        if (const auto* definition =
                item_assets_.FindMaterial(material.id); definition != nullptr) {
            row.sprite_index = scene_.FindSpriteIndex(definition->sprite);
        }
        data.material_rows.push_back(std::move(row));
    }
    std::sort(data.material_rows.begin(), data.material_rows.end(),
              [](const auto& a, const auto& b) {
                  return a.name < b.name;
              });
    data.workshop_status = workshop_status_;
    data.workshop_station =
        game_ui_.Screen() == soulknight::game::ui::UIScreen::Workshop &&
        workshop_status_.rfind("WEAPON STATION", 0) == 0;
    data.workshop_selected = game_ui_.WorkshopSelection();
    for (const auto& recipe : workshop_.Recipes()) {
        soulknight::game::ui::WorkshopRecipeView view;
        view.weapon_id = recipe.weapon_id;
        view.name = recipe.name;
        std::size_t weapon_index = weapons_.Count();
        for (std::size_t i = 0; i < weapons_.Count(); ++i) {
            if (weapons_.Definitions()[i].id == recipe.weapon_id) {
                weapon_index = i;
                break;
            }
        }
        if (weapon_index < weapons_.Count()) {
            const auto& definition = weapons_.Definitions()[weapon_index];
            view.sprite_index = definition.visual_sprite_index;
            view.damage = definition.damage;
            view.energy_cost = definition.EnergyCost();
            if (!definition.display_name.empty()) view.name = definition.display_name;
        }
        view.blueprint_required =
            workshop_.RequiresBlueprint(recipe.weapon_id);
        view.blueprint_unlocked =
            workshop_.HasBlueprint(recipe.weapon_id);
        view.affordable = weapon_index < weapons_.Count() &&
            workshop_.CanCraft(recipe.weapon_id);
        for (const auto& material : recipe.materials) {
            char line[160];
            std::snprintf(line, sizeof(line), "%s  x%d  (have %d)",
                          workshop_.MaterialName(material.id).c_str(),
                          material.count,
                          workshop_.MaterialCount(material.id));
            view.materials.push_back(line);
        }
        data.workshop_recipes.push_back(std::move(view));
    }
    data.selected_pet = static_cast<int>(pet_.SelectedIndex());
    for (const auto& definition : pet_.Definitions()) {
        data.pet_names.push_back(definition.display_name);
        data.pet_sprites.push_back(definition.body_sprite_index);
    }
    soulknight::game::ui::UIRenderContext context;
    context.camera = &render_camera_;
    context.batch = &sprite_batch_;
    context.text = &dev_ui_.Text();
    context.scene = &scene_;
    ui_renderer_.Begin(context);
    game_ui_.Draw(ui_renderer_, data);
}

void Application::PollEvents() {
    input_manager_.BeginFrame();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        const bool was_minimized = window_.IsMinimized();
        const bool was_focused = window_.IsFocused();

        if (event.type == SDL_EVENT_QUIT) {
            LOG_INFO_APP("SDL quit event");
            running_ = false;
        }

        if (HandleUiDesignEvent(event)) {
            window_.HandleEvent(event);
            input_manager_.ProcessEvent(event);
            continue;
        }
        if (HandleHubSuggestionEvent(event)) {
            window_.HandleEvent(event);
            input_manager_.ProcessEvent(event);
            continue;
        }
        HandleMainMenuPointer(event);

        const bool key_down = event.type == SDL_EVENT_KEY_DOWN &&
                              !event.key.repeat;
        if (key_down && event.key.scancode == SDL_SCANCODE_ESCAPE) {
            if (interactive_test_.Active()) {
                FinalizeInteractiveTest("ABORTED", "user aborted with Escape");
                running_ = false;
            } else if (asset_lab_.IsOpen()) {
                asset_lab_.Close();
            } else if (dev_ui_.MenuOpen()) {
                dev_ui_.CloseMenu();
            } else if (game_ui_.Screen() == soulknight::game::ui::UIScreen::Settings) {
                game_ui_.Back();
            } else if (game_ui_.Screen() == soulknight::game::ui::UIScreen::SaveLoad ||
                       game_ui_.Screen() == soulknight::game::ui::UIScreen::Trader) {
                game_ui_.Back();
            } else if (game_ui_.IsPaused()) {
                PlayUiSound("pause");
                game_ui_.Resume();
            } else if (game_ui_.IsMainMenu()) {
                running_ = false;
            } else {
                PlayUiSound("pause");
                game_ui_.OpenPause();
            }
        }

        if (key_down && scene_mode_) {
            HandleSceneKeyDown(event.key.scancode,
                               static_cast<SDL_Keymod>(event.key.mod));
        }

        window_.HandleEvent(event);
        input_manager_.ProcessEvent(event);

        if (event.type == SDL_EVENT_WINDOW_RESIZED ||
            event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            LOG_INFO_APP("Window geometry event: {}x{} logical, {}x{} pixels",
                         window_.GetWidth(), window_.GetHeight(),
                         window_.GetPixelWidth(), window_.GetPixelHeight());
        }
        if (was_minimized != window_.IsMinimized()) {
            LOG_INFO_APP("Window minimized: {}", BooleanText(window_.IsMinimized()));
        }
        if (was_focused != window_.IsFocused()) {
            LOG_INFO_APP("Window focused: {}", BooleanText(window_.IsFocused()));
        }
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            LOG_INFO_APP("Window close requested");
        }

        if (window_.ShouldClose()) {
            running_ = false;
        }
    }

    input_manager_.EndFrame();

    // Event delivery is normally sufficient, but SDL/compositor combinations
    // vary in focus/restore notification. Reconcile against SDL's flags once per
    // frame; this remains platform state, not gameplay input.
    window_.SyncRuntimeState();
}

void Application::RebuildCollisionWorld() {
    collision_world_ = {};
    if (scene_.has_room_topology) {
        if (scene_.collision_bounds.w > 0.0f && scene_.collision_bounds.h > 0.0f) {
            collision_world_.SetBounds(engine::collision::Aabb{
                {scene_.collision_bounds.x, scene_.collision_bounds.y},
                {scene_.collision_bounds.x + scene_.collision_bounds.w,
                 scene_.collision_bounds.y + scene_.collision_bounds.h}
            });
        } else {
            collision_world_.SetBounds(MakeFallbackCollisionBounds(camera_.position));
        }
        for (const engine::assets::CollisionBox& solid : scene_.collision_solids) {
            collision_world_.AddSolid(ToAabb(solid));
        }
        obstacles_.AddColliders(collision_world_);
        room_doors_.AddColliders(collision_world_);
        return;
    }

    if (scene_.collision_bounds.w > 0.0f && scene_.collision_bounds.h > 0.0f) {
        collision_world_.SetBounds(engine::collision::Aabb{
            {scene_.collision_bounds.x, scene_.collision_bounds.y},
            {scene_.collision_bounds.x + scene_.collision_bounds.w,
             scene_.collision_bounds.y + scene_.collision_bounds.h}
        });
    } else {
        collision_world_.SetBounds(MakeFallbackCollisionBounds(camera_.position));
    }
    for (const engine::assets::CollisionBox& solid : scene_.collision_solids) {
        collision_world_.AddSolid(ToAabb(solid));
    }
    obstacles_.AddColliders(collision_world_);
    boxes_.AddColliders(collision_world_);
    if (scene_.id == "r_sell_visual" && seller_.Configured()) {
        for (const auto& offer : seller_.Offers()) {
            collision_world_.AddSolid({
                {offer.position.x - 0.72f, offer.position.y - 0.72f},
                {offer.position.x + 0.72f, offer.position.y + 0.28f}});
        }
        const engine::Vec2 base = scene_.spawn;
        collision_world_.AddSolid({
            {base.x - 1.35f, base.y + 2.75f},
            {base.x + 1.35f, base.y + 4.05f}});
    }
    for (const auto& chest : item_world_.Chests()) {
        const auto* definition = item_assets_.FindChest(chest.definition);
        if (definition == nullptr) continue;
        const engine::collision::Aabb box{
            {chest.position.x + definition->collider_offset.x -
                 definition->collider_half_extents.x,
             chest.position.y + definition->collider_offset.y -
                 definition->collider_half_extents.y},
            {chest.position.x + definition->collider_offset.x +
                 definition->collider_half_extents.x,
             chest.position.y + definition->collider_offset.y +
                 definition->collider_half_extents.y}};
        collision_world_.AddSolid(box);
    }
    room_doors_.AddColliders(collision_world_);
}

void Application::ToggleAllDoors() {
    if (room_doors_.Count() == 0) return;
    room_doors_.ToggleAll();
    RebuildCollisionWorld();
    const bool target = room_doors_.TargetOpen();
    LOG_INFO_APP("Door lifecycle: {} (animation started, {} doors)",
                 target ? "OpenDoor()" : "CloseDoor()", room_doors_.Count());
    for (std::size_t i = 0; i < room_doors_.Count(); ++i) {
        const soulknight::game::room::Door& door = room_doors_.At(i);
        LOG_INFO_APP("  door {}: state={} blocking={} t={:.3f}/{:.3f}",
                     door.Data().id,
                     soulknight::game::room::DoorStateName(door.State()),
                     BooleanText(door.IsBlocking()), door.AnimationTime(),
                     door.Data().close_animation.duration);
    }
}

void Application::CloseAllDoors() {
    if (room_doors_.Count() == 0) return;
    room_doors_.CloseAll();
    RebuildCollisionWorld();
    LOG_INFO_APP("Door lifecycle: CloseDoor() for {} doors", room_doors_.Count());
    for (std::size_t i = 0; i < room_doors_.Count(); ++i) {
        const soulknight::game::room::Door& door = room_doors_.At(i);
        LOG_INFO_APP("  door {}: state={} blocking={} t={:.3f}/{:.3f}",
                     door.Data().id,
                     soulknight::game::room::DoorStateName(door.State()),
                     BooleanText(door.IsBlocking()), door.AnimationTime(),
                     door.Data().close_animation.duration);
    }
}

void Application::SwitchCharacter(std::size_t index) {
    if (player_characters_.Count() == 0) return;
    if (!player_characters_.Select(index)) return;
    locomotion_animator_.Configure(player_characters_.IdleClip(),
                                   player_characters_.RunClip(),
                                   player_characters_.DeathClip(),
                                   player_characters_.HitClip());
    last_animation_sprite_.clear();
    last_animation_state_ = engine::animation::LocomotionState::Idle;
    animation_logs_remaining_ = 8;
    ApplyActiveCharacterStats();
    ConfigureAbilities();
    const auto& character = player_characters_.Active();
    if (!character.starting_weapon.empty() && weapons_.Count() > 0) {
        for (std::size_t i = 0; i < weapons_.Count(); ++i) {
            if (weapons_.Definitions()[i].id == character.starting_weapon) {
                weapon_inventory_.SwitchSlot(0);
                (void)weapon_inventory_.EquipActive(i);
                ConfigureActiveWeapon();
                ConfigureSecondaryWeapon();
                break;
            }
        }
    }
    LOG_INFO_APP("Character switch: [{}] {} (controller {}, body={}, idle={}, run={})",
                 player_characters_.ActiveIndex() + 1, character.id,
                 character.controller_name, character.body_sprite_id,
                 character.idle_animation_id, character.run_animation_id);
}

void Application::ApplyActiveCharacterStats() {
    if (player_characters_.Count() == 0) return;
    const auto& character = player_characters_.Active();
    if (character.max_hp > 0) {
        player_health_.Reset(character.max_hp, character.max_armor, 0);
        player_health_.camp = scene_.player_stats.max_hp > 0
            ? scene_.player_stats.camp : 1;
        player_dead_ = false;
        weapon_.Release();
        attack_held_ = false;
    }
    if (character.max_energy > 0) {
        player_energy_.Reset(character.max_energy, character.max_energy);
    }
    if (character.speed > 0.0f) {
        player_.SetSpeedProfile(character.speed, character.speed_rate);
    }
    LOG_INFO_APP("Character stats: {} hp={} armor={} energy={} speed={:.2f}",
                 character.id, player_health_.max_hp,
                 player_health_.max_armor, player_energy_.max_energy,
                 character.speed);
}

void Application::CycleCharacter(int delta) {
    if (player_characters_.Count() == 0) return;
    player_characters_.Cycle(delta);
    SwitchCharacter(player_characters_.ActiveIndex());
}

void Application::CycleObstacleVariant() {
    if (obstacles_.Variants().empty()) return;
    obstacles_.CycleNext();
    RebuildCollisionWorld();
    const auto& variant = obstacles_.ActiveVariant();
    LOG_INFO_APP("Obstacle switch: [{}] {} path_id={} visuals={} collider_cells={}",
                 obstacles_.ActiveIndex() + 1, variant.name, variant.path_id,
                 variant.visuals.size(), obstacles_.ColliderCellCount());
}

void Application::UpdateAimInput(const platform::InputSnapshot* override) {
    if (aim_mode_ == soulknight::game::player::AimMode::Auto) return;
    const platform::InputSnapshot& snapshot =
        override != nullptr ? *override : input_manager_.GetSnapshot();
    // Reference path: RGController.AutoLock consumes the right-stick / locked
    // target direction into fixed_angle; Gun001.Attack uses that angle.
    // NATIVE TEST BINDINGS: right stick or mouse cursor provide the aim vector.
    // Body facing is resolved separately from the aim vector by
    // ResolveAimFacing, so movement and aim stay independent.
    engine::Vec2 aim = aim_direction_;
    const float stick_magnitude =
        std::sqrt(snapshot.aim_x * snapshot.aim_x +
                  snapshot.aim_y * snapshot.aim_y);
    bool have_aim = false;
    engine::Vec2 digital_aim{0.0f, 0.0f};
    if (snapshot.IsHeld(platform::Action::AimUp)) digital_aim.y += 1.0f;
    if (snapshot.IsHeld(platform::Action::AimDown)) digital_aim.y -= 1.0f;
    if (snapshot.IsHeld(platform::Action::AimLeft)) digital_aim.x -= 1.0f;
    if (snapshot.IsHeld(platform::Action::AimRight)) digital_aim.x += 1.0f;
    if (digital_aim.LengthSquared() > 0.0f) {
        aim = digital_aim;
        have_aim = true;
    } else if (stick_magnitude > 0.15f) {
        aim = {snapshot.aim_x, snapshot.aim_y};
        have_aim = true;
    } else {
        if (snapshot.mouse_delta.x != 0.0f || snapshot.mouse_delta.y != 0.0f) {
            mouse_aim_valid_ = true;
        }
        if (mouse_aim_valid_) {
            const engine::Vec2 mouse = logical_viewport_.WindowPointToLogical(
                snapshot.mouse_position);
            last_mouse_logical_ = mouse;
            const engine::Vec2 world = camera_.ScreenToWorld(mouse);
            last_mouse_world_ = world;
            const engine::Vec2 delta = world - player_.Position();
            if (delta.LengthSquared() > 0.01f) {
                aim = delta;
                have_aim = true;
            }
        }
    }
    if (!have_aim && movement_input_.IsActive()) {
        aim = movement_input_.direction;
        have_aim = true;
    }
    if (have_aim && aim.LengthSquared() > 1e-8f) {
        aim_direction_ = aim.Normalized();
    }
}

void Application::CycleAssetLabCategory(int delta) {
    asset_lab_.NextCategory(delta);
    CycleAssetLabItem(0);
    LOG_INFO_APP("Asset lab category: {}", asset_lab_.CategoryName());
}

void Application::CycleAssetLabItem(int delta) {
    const std::size_t count = asset_lab_.ItemCount(scene_);
    if (count == 0) {
        asset_lab_.SetItemIndex(0);
        return;
    }
    std::size_t current = std::min(asset_lab_.ItemIndex(), count - 1);
    if (delta != 0) {
        const int count_i = static_cast<int>(count);
        int next = (static_cast<int>(current) + delta) % count_i;
        if (next < 0) next += count_i;
        current = static_cast<std::size_t>(next);
    }
    asset_lab_.SetItemIndex(current);
    LOG_INFO_APP("Asset lab: {} [{}] {}", asset_lab_.CategoryName(),
                 current + 1, asset_lab_.ItemId(scene_));
}

void Application::ActivateAssetLabSelection() {
    const std::size_t count = asset_lab_.ItemCount(scene_);
    if (count == 0) {
        LOG_INFO_APP("Asset lab: {} has no integrated assets",
                     asset_lab_.CategoryName());
        return;
    }
    const std::size_t index = std::min(asset_lab_.ItemIndex(), count - 1);
    using soulknight::game::ui::AssetCategory;
    switch (asset_lab_.Category()) {
        case AssetCategory::Characters:
            SwitchCharacter(index);
            LOG_INFO_APP("Asset lab: activated character {}", index);
            break;
        case AssetCategory::Weapons:
            if (weapon_inventory_.EquipActive(index)) {
                ConfigureActiveWeapon();
            }
            LOG_INFO_APP("Asset lab: equipped weapon {}", index);
            break;
        case AssetCategory::Enemies: {
            const engine::Vec2 position =
                player_.Position() + engine::Vec2{2.0f, 0.0f};
            if (enemies_.SpawnEnemy(index, position)) {
                LOG_INFO_APP("Asset lab: spawned enemy {}", index);
            }
            break;
        }
        case AssetCategory::Obstacles:
            if (obstacles_.Select(index)) {
                RebuildCollisionWorld();
                LOG_INFO_APP("Asset lab: selected obstacle variant {}", index);
            }
            break;
        default:
            LOG_INFO_APP("Asset lab: preview-only asset {}[{}]",
                         asset_lab_.CategoryName(), index + 1);
            break;
    }
}

void Application::SpawnSelectedEnemy(int count) {
    if (enemies_.DefinitionCount() == 0) {
        LOG_WARN_APP("Enemy spawn requested but the scene has no enemy definitions");
        return;
    }
    const std::size_t definition =
        std::min(dev_ui_.SelectedIndex(), enemies_.DefinitionCount() - 1);
    const engine::Vec2 base = player_.Position();
    const engine::Vec2 offsets[5] = {
        {2.0f, 0.0f}, {-2.0f, 0.0f}, {0.0f, 2.0f},
        {0.0f, -2.0f}, {2.0f, 2.0f}
    };
    const int spawn_count = std::max(1, count);
    for (int i = 0; i < spawn_count; ++i) {
        const engine::Vec2 offset =
            offsets[static_cast<std::size_t>(i) % 5U] +
            engine::Vec2{0.15f * static_cast<float>(i / 5),
                         0.15f * static_cast<float>(i / 5)};
        if (!enemies_.SpawnEnemy(definition, base + offset)) {
            LOG_WARN_APP("Enemy spawn failed for definition {}", definition);
            break;
        }
    }
    LOG_INFO_APP("Developer enemy spawner: definition={} spawned={} total={}",
                 enemies_.DefinitionAt(definition).id, spawn_count,
                 enemies_.Enemies().size());
}

void Application::SaveGameToDisk() {
    if (save_manager_ == nullptr || weapons_.Count() == 0) return;
    soulknight::game::save::SaveData data;
    data.game_version = LITEKNIGHT_VERSION;
    data.scene_id = scene_.id;
    data.character_id = player_characters_.Count() > 0
        ? player_characters_.Active().id : scene_.player_stats.character_id;
    data.weapon_slots[0] = weapons_.Definitions()[weapon_inventory_.Slot(0)].id;
    data.weapon_slots[1] = weapons_.Definitions()[weapon_inventory_.Slot(1)].id;
    data.active_slot = static_cast<int>(weapon_inventory_.ActiveSlot());
    data.position_x = player_.Position().x;
    data.position_y = player_.Position().y;
    data.hp = player_health_.hp;
    data.max_hp = player_health_.max_hp;
    data.armor = player_health_.armor;
    data.max_armor = player_health_.max_armor;
    data.energy = player_energy_.energy;
    data.max_energy = player_energy_.max_energy;
    data.coins = player_coins_;
    data.orbs = player_orbs_;
    data.dead = player_health_.dead;
    data.playtime_seconds = static_cast<std::int64_t>(simulation_time_);
    for (const auto& character : scene_.characters) {
        data.unlocked_characters.push_back(character.id);
    }
    data.fullscreen = window_.IsFullscreen();
    data.hub_visited = scene_.is_hub;
    data.auto_aim = aim_mode_ == soulknight::game::player::AimMode::Auto;
    data.reduced_weapon_effects = reduced_weapon_effects_;
    data.living_room_suggestions = living_room_suggestions_;
    data.map_seed = map_seed_;
    data.campaign_level = biome_.Current() != nullptr
        ? biome_.Current()->id : std::string("1-1");
    data.trader_stock_seed = trader_stock_seed_;
    for (std::size_t i = 0; i < trader_offers_.size(); ++i) {
        if (trader_offers_[i].sold) {
            data.sold_trader_offers.push_back(static_cast<int>(i));
        }
    }
    data.pet_id = active_pet_id_;
    data.cleared_rooms.assign(cleared_rooms_.begin(), cleared_rooms_.end());
    data.chest_spawned_rooms.assign(chest_spawned_rooms_.begin(),
                                    chest_spawned_rooms_.end());
    data.sold_shop_items.assign(sold_shop_items_.begin(),
                                sold_shop_items_.end());
    data.active_statue = active_statue_id_;
    data.active_mercenary = active_mercenary_id_;
    data.mercenary_level = mercenary_level_;
    data.active_mount = active_mount_id_;
    data.pot_empty = pot_empty_state_;
    data.unlocked_blueprints = workshop_.UnlockedBlueprints();
    data.materials.clear();
    for (const auto& material : workshop_.SnapshotMaterials()) {
        data.materials.push_back({material.id, material.count});
    }
    for (const auto& plot : plant_system_.Plots()) {
        if (!plot.species.empty()) data.plants.push_back(plot.species);
    }
    data.plant_states = plant_system_.SerializePlots();
    data.seed_stock = plant_system_.SerializeSeeds();
    for (const auto& snapshot : item_world_.SnapshotRooms()) {
        soulknight::game::save::SaveRoomItems room;
        room.id = snapshot.id;
        for (const auto& chest : snapshot.chests) {
            soulknight::game::save::SaveChestState saved;
            saved.definition = chest.definition;
            saved.x = chest.position.x;
            saved.y = chest.position.y;
            saved.state = static_cast<int>(chest.state);
            saved.timer = chest.timer;
            saved.loot_spawned = chest.loot_spawned;
            saved.pending_weapon = chest.pending_weapon;
            saved.pending_weapons = chest.pending_weapons;
            room.chests.push_back(std::move(saved));
        }
        for (const auto& coin : snapshot.coins) {
            soulknight::game::save::SaveCoinState saved;
            saved.definition = coin.definition;
            saved.x = coin.position.x;
            saved.y = coin.position.y;
            saved.active = coin.active;
            room.coins.push_back(std::move(saved));
        }
        data.rooms.push_back(std::move(room));
    }
    std::string error;
    if (save_manager_->Save(data, &error)) {
        LOG_INFO_APP("Saved native game state: {}",
                     save_manager_->SlotPath().string());
    } else {
        LOG_ERROR_APP("Save failed: {}", error);
    }
    RefreshSaveSummary();
}

void Application::RefreshSaveSummary() {
    if (save_manager_ == nullptr) {
        save_slot_summary_ = "SAVE MANAGER UNAVAILABLE";
        return;
    }
    if (!save_manager_->Exists()) {
        save_slot_summary_ = "NO NATIVE SAVE";
        return;
    }
    soulknight::game::save::SaveData data;
    std::string error;
    if (!save_manager_->Load(&data, &error)) {
        save_slot_summary_ = "SAVE UNREADABLE: " + error;
        return;
    }
    const auto weapon_name = [&](const std::string& id) {
        if (id.empty()) return std::string("-");
        const auto* entry = weapon_catalogue_.Find(id);
        return entry != nullptr ? entry->display_name : id;
    };
    save_slot_summary_ =
        "CHARACTER " + (data.character_id.empty() ? "?" : data.character_id) +
        "   LEVEL " + (data.scene_id.empty() ? "?" : data.scene_id) +
        "   //   WEAPONS " + weapon_name(data.weapon_slots[0]) +
        " / " + weapon_name(data.weapon_slots[1]) +
        "   //   HP " + std::to_string(data.hp) + "/" +
        std::to_string(data.max_hp) +
        " ARMOR " + std::to_string(data.armor) + "/" +
        std::to_string(data.max_armor) +
        " ENERGY " + std::to_string(data.energy) + "/" +
        std::to_string(data.max_energy) +
        "   //   COINS " + std::to_string(data.coins) +
        " ORBS " + std::to_string(data.orbs) +
        " PLAYTIME " + std::to_string(data.playtime_seconds) + "s" +
        " SEED " + std::to_string(data.map_seed);
}

void Application::PlayUiSound(const std::string& id) {
    if (!audio_available_) return;
    const std::filesystem::path path =
        filesystem_.ResolvePath("audio/" + id + ".wav");
    if (path.empty() || !std::filesystem::exists(path)) return;
    audio_voices_.push_back(audio_engine_.PlayStatic(
        path.string(), platform::AudioEngine::Bus::UI, 0.75f));
}

void Application::PlayEffect(const std::string& id, float volume) {
    if (!audio_available_) return;
    const std::filesystem::path path =
        filesystem_.ResolvePath("audio/" + id + ".wav");
    if (path.empty() || !std::filesystem::exists(path)) return;
    audio_voices_.push_back(audio_engine_.PlayStatic(
        path.string(), platform::AudioEngine::Bus::SFX, volume));
}

void Application::PlayMusic(const std::string& id) {
    if (!audio_available_ || active_music_ == id) return;
    const std::filesystem::path path =
        filesystem_.ResolvePath("audio/" + id + ".wav");
    active_music_ = id;
    if (path.empty() || !std::filesystem::exists(path)) return;
    music_ = audio_engine_.PlayStreaming(
        path.string(), platform::AudioEngine::Bus::Music, 0.42f, true);
    LOG_INFO_APP("Music: {}", id);
}

void Application::UpdateAudio() {
    if (!audio_available_) return;
    audio_voices_.erase(
        std::remove_if(audio_voices_.begin(), audio_voices_.end(),
                       [](const platform::AudioEngine::SoundHandle& voice) {
                           return !voice.IsPlaying();
                       }),
        audio_voices_.end());
}

void Application::LoadGameFromDisk() {
    if (save_manager_ == nullptr) return;
    soulknight::game::save::SaveData data;
    std::string error;
    if (!save_manager_->Load(&data, &error)) {
        LOG_ERROR_APP("Load failed: {}", error);
        return;
    }
    if (!error.empty()) {
        LOG_WARN_APP("Load recovered with warning: {}", error);
    }
    if (multi_room_world_) {
        const std::string saved_level =
            data.campaign_level.empty() ? "1-1" : data.campaign_level;
        const bool level_changed =
            biome_.Current() == nullptr || biome_.Current()->id != saved_level;
        if (level_changed) {
            if (!biome_.Select(saved_level)) {
                LOG_WARN_APP("Save references unknown campaign level: {}",
                             saved_level);
            }
        }
        if (level_changed || data.map_seed != map_seed_) {
            if (!LoadWorldRooms(data.map_seed)) {
                LOG_WARN_APP("Save world rebuild failed for level {} seed {}",
                             saved_level, data.map_seed);
            }
        }
        if (data.scene_id != scene_.id &&
            room_manager_.Find(data.scene_id) != nullptr) {
            if (!ActivateRoom(data.scene_id, "")) {
                LOG_ERROR_APP("Save scene activation failed: {}", data.scene_id);
                return;
            }
        }
    }
    if (data.scene_id != scene_.id) {
        LOG_ERROR_APP("Save scene mismatch: save={} current={}",
                      data.scene_id, scene_.id);
        return;
    }
    std::size_t character_index = scene_.characters.size();
    for (std::size_t i = 0; i < scene_.characters.size(); ++i) {
        if (scene_.characters[i].id == data.character_id) {
            character_index = i;
            break;
        }
    }
    if (character_index == scene_.characters.size()) {
        LOG_ERROR_APP("Save references unknown character: {}",
                      data.character_id);
        return;
    }
    std::array<std::size_t, 2> weapon_indices{};
    for (std::size_t slot = 0; slot < 2; ++slot) {
        std::size_t found = weapons_.Count();
        for (std::size_t i = 0; i < weapons_.Count(); ++i) {
            if (weapons_.Definitions()[i].id == data.weapon_slots[slot]) {
                found = i;
                break;
            }
        }
        if (found == weapons_.Count()) {
            LOG_ERROR_APP("Save references unknown weapon: {}",
                          data.weapon_slots[slot]);
            return;
        }
        weapon_indices[slot] = found;
    }

    SwitchCharacter(character_index);
    player_.SetPosition({data.position_x, data.position_y});
    camera_.position = {data.position_x, data.position_y};
    player_health_.Reset(data.max_hp, data.max_armor, 0);
    player_health_.hp = data.hp;
    player_health_.armor = data.armor;
    player_health_.dead = data.dead || data.hp <= 0;
    player_health_.can_hurt = !player_health_.dead;
    player_energy_.Reset(data.max_energy, data.energy);
    player_coins_ = std::max(0, data.coins);
    player_orbs_ = std::max(0, data.orbs);
    active_pet_id_ = data.pet_id;
    if (!active_pet_id_.empty()) pet_.SelectSaved(active_pet_id_);
    std::vector<soulknight::game::hub::CraftMaterial> materials;
    for (const auto& material : data.materials) {
        materials.push_back({material.id, material.count});
    }
    workshop_.RestoreMaterials(materials);
    workshop_.SetUnlockedBlueprints(data.unlocked_blueprints);
    if (!plant_system_.Plots().empty()) {
        (void)plant_system_.RestorePlots(data.plant_states);
        (void)plant_system_.RestoreSeeds(data.seed_stock);
        selected_plant_seed_ = plant_system_.FirstAvailableSeed();
    }
    cleared_rooms_.clear();
    cleared_rooms_.insert(data.cleared_rooms.begin(), data.cleared_rooms.end());
    chest_spawned_rooms_.clear();
    chest_spawned_rooms_.insert(data.chest_spawned_rooms.begin(),
                                data.chest_spawned_rooms.end());
    sold_shop_items_.clear();
    sold_shop_items_.insert(data.sold_shop_items.begin(),
                            data.sold_shop_items.end());
    active_statue_id_ = data.active_statue;
    active_mercenary_id_ = data.active_mercenary;
    mercenary_level_ = std::max(1, data.mercenary_level);
    active_mount_id_ = data.active_mount;
    trader_stock_seed_ = data.trader_stock_seed != 0
        ? data.trader_stock_seed : trader_stock_seed_;
    ConfigureTraderOffers();
    for (const int index : data.sold_trader_offers) {
        if (index >= 0 &&
            static_cast<std::size_t>(index) < trader_offers_.size()) {
            trader_offers_[static_cast<std::size_t>(index)].sold = true;
        }
    }
    mount_speed_bonus_ = active_mount_id_.empty() ? 0.0f : 0.35f;
    pot_empty_state_ = data.pot_empty;
    reduced_weapon_effects_ = data.reduced_weapon_effects;
    living_room_suggestions_ = data.living_room_suggestions;
    mercenary_spawned_ = false;
    std::vector<soulknight::game::items::RoomItemSnapshot> room_items;
    for (const auto& room : data.rooms) {
        soulknight::game::items::RoomItemSnapshot snapshot;
        snapshot.id = room.id;
        for (const auto& chest : room.chests) {
            soulknight::game::items::ChestInstance saved;
            saved.definition = chest.definition;
            saved.position = {chest.x, chest.y};
            saved.state = static_cast<soulknight::game::items::ChestState>(
                chest.state);
            saved.timer = chest.timer;
            saved.loot_spawned = chest.loot_spawned;
            saved.pending_weapon = chest.pending_weapon;
            saved.pending_weapons = chest.pending_weapons;
            snapshot.chests.push_back(std::move(saved));
        }
        for (const auto& coin : room.coins) {
            soulknight::game::items::CoinInstance saved;
            saved.definition = coin.definition;
            saved.position = {coin.x, coin.y};
            saved.active = coin.active;
            snapshot.coins.push_back(std::move(saved));
        }
        room_items.push_back(std::move(snapshot));
    }
    item_world_.RestoreRooms(room_items);
    aim_mode_ = data.auto_aim ? soulknight::game::player::AimMode::Auto
                              : soulknight::game::player::AimMode::Manual;
    if (data.fullscreen != window_.IsFullscreen()) {
        (void)window_.SetFullscreen(data.fullscreen);
    }
    player_dead_ = player_health_.dead;
    attack_held_ = false;
    weapon_.Release();
    projectiles_.Clear();

    weapon_inventory_.Configure(weapons_.Count());
    weapon_inventory_.SwitchSlot(0);
    (void)weapon_inventory_.EquipActive(weapon_indices[0]);
    weapon_inventory_.SwitchSlot(1);
    (void)weapon_inventory_.EquipActive(weapon_indices[1]);
    weapon_inventory_.SwitchSlot(static_cast<std::size_t>(data.active_slot));
    ConfigureActiveWeapon();
    ConfigureSecondaryWeapon();
    LOG_INFO_APP("Loaded native game state: {} character={} slot={} ",
                 save_manager_->SlotPath().string(), data.character_id,
                 data.active_slot + 1);
}

void Application::SpawnWeaponPickupAt(std::size_t weapon_index,
                                      const engine::Vec2& position) {
    if (weapon_index >= weapons_.Count()) return;
    if (weapon_pickups_.Spawn(weapon_index, position)) {
        LOG_INFO_APP("Weapon pickup spawned: {} at ({:.2f},{:.2f}) [E to take]",
                     weapons_.Definitions()[weapon_index].id, position.x,
                     position.y);
    }
}

void Application::SpawnWeaponPickup(std::size_t weapon_index) {
    SpawnWeaponPickupAt(weapon_index,
                        {player_.Position().x + 1.4f,
                         player_.Position().y - 0.1f});
}

const engine::assets::InteractionMarker* Application::NearbyInteraction() const {
    const engine::assets::InteractionMarker* best = nullptr;
    float best_distance = 0.0f;
    for (const auto& marker : scene_.interaction_markers) {
        // The hub hides the player's duplicate hero NPC and its dialogue
        // marker, matching the reference character occupancy.
        if (!marker.character_id.empty() && player_characters_.Count() > 0 &&
            marker.character_id == player_characters_.Active().id) {
            continue;
        }
        if (marker.kind == "mineral" || marker.kind == "statue") {
            const auto object = std::find_if(
                special_objects_.begin(), special_objects_.end(),
                [&](const SpecialObjectInstance& candidate) {
                    return candidate.id == marker.item_id;
                });
            if (object == special_objects_.end() || !object->active ||
                object->used) {
                continue;
            }
        }
        if (marker.kind == "seller" && marker.seller_index >= 0) {
            const auto* offer = seller_.FindByIndex(
                static_cast<std::size_t>(marker.seller_index));
            if (offer == nullptr || offer->sold) continue;
        }
        const float radius = marker.radius > 0.0f ? marker.radius : 2.0f;
        const float distance =
            (marker.position - player_.Position()).LengthSquared();
        if (distance <= radius * radius &&
            (best == nullptr || distance < best_distance)) {
            best = &marker;
            best_distance = distance;
        }
    }
    return best;
}

bool Application::TryOpenChest() {
    if (!item_assets_.IsLoaded()) return false;
    if (item_world_.OpenNearestChest(player_.Position(), 1.8f)) {
        PlayEffect("chest_open");
        LOG_INFO_APP("Chest opened: {}", scene_.id);
        return true;
    }
    return false;
}

void Application::ConfigureRoomItems() {
    if (!scene_mode_ || !item_assets_.IsLoaded()) return;
    std::vector<std::string> available;
    available.reserve(weapons_.Definitions().size());
    for (const auto& definition : weapons_.Definitions()) {
        available.push_back(definition.id);
    }
    item_world_.SetAvailableWeapons(std::move(available));
    const bool dungeon = !scene_.is_hub &&
                         scene_.id.rfind("r_", 0) == 0;
    item_world_.SetChestLevel(
        biome_.Current() != nullptr ? biome_.Current()->chest_level : 1);
    item_world_.SetLootSeed(static_cast<std::uint32_t>(map_seed_));
    seller_.Reset();
    scene_.interaction_markers.erase(
        std::remove_if(scene_.interaction_markers.begin(),
                       scene_.interaction_markers.end(),
                       [](const engine::assets::InteractionMarker& marker) {
                           return marker.kind == "seller";
                       }),
        scene_.interaction_markers.end());
    if (scene_.id == "r_sell_visual" &&
        seller_.Configure(item_assets_, scene_,
                          biome_.Current() != nullptr
                              ? biome_.Current()->chest_level : 1,
                          map_seed_, &weapon_catalogue_, scene_.id)) {
        seller_.RestoreSold(std::vector<std::string>(
            sold_shop_items_.begin(), sold_shop_items_.end()));
        for (std::size_t i = 0; i < seller_.Offers().size(); ++i) {
            const auto& offer = seller_.Offers()[i];
            if (offer.sold) continue;
            engine::assets::InteractionMarker marker;
            marker.id = "seller_" + std::to_string(i);
            marker.kind = "seller";
            marker.label = offer.display_name + "  -" +
                std::to_string(offer.price);
            marker.position = offer.position;
            marker.radius = 2.4f;
            marker.seller_index = static_cast<int>(i);
            marker.price = offer.price;
            marker.item_id = offer.item_id;
            scene_.interaction_markers.push_back(std::move(marker));
        }
        for (auto& offer : seller_.OffersMutable()) {
            if (!offer.sprite.empty() || offer.weapon_id.empty()) continue;
            for (const auto& definition : weapons_.Definitions()) {
                if (definition.id != offer.weapon_id) continue;
                if (definition.visual_sprite_index >= 0 &&
                    static_cast<std::size_t>(definition.visual_sprite_index) <
                        scene_.sprites.size()) {
                    offer.sprite = scene_.sprites[static_cast<std::size_t>(
                        definition.visual_sprite_index)].id;
                }
                break;
            }
        }
        LOG_INFO_APP("Seller: {} recovered offers in {}", seller_.Offers().size(),
                     scene_.id);
    }
    special_objects_.clear();
    scene_.interaction_markers.erase(
        std::remove_if(scene_.interaction_markers.begin(),
                       scene_.interaction_markers.end(),
                       [](const engine::assets::InteractionMarker& marker) {
                           return marker.kind == "statue" ||
                                  marker.kind == "mineral" ||
                                  marker.kind == "mercenary" ||
                                  marker.kind == "mercenary_trainer" ||
                                  marker.kind == "mount";
                       }),
        scene_.interaction_markers.end());
    const bool special_room =
        scene_.id == "r_statue_visual" ||
        scene_.id == "r_mineral_visual" ||
        scene_.id == "r_mercenary_visual" ||
        scene_.id == "r_mount_visual";
    if (special_room && item_assets_.IsLoaded()) {
        std::uint32_t seed = static_cast<std::uint32_t>(map_seed_) ^
            0x53504543u;
        for (const char c : scene_.id) {
            seed ^= static_cast<unsigned char>(c);
            seed *= 16777619u;
        }
        const soulknight::game::items::SpecialObjectDefinition* chosen = nullptr;
        std::string marker_kind;
        int marker_price = 0;
        if (scene_.id == "r_statue_visual") {
            std::vector<const soulknight::game::items::SpecialObjectDefinition*> statues;
            for (const auto& object : item_assets_.SpecialObjects()) {
                if (object.role == "statue") statues.push_back(&object);
            }
            if (!statues.empty()) {
                chosen = statues[seed % statues.size()];
                marker_kind = "statue";
                marker_price = chosen->price;
            }
        } else if (scene_.id == "r_mineral_visual") {
            const bool gold = (seed & 1u) == 0;
            chosen = item_assets_.FindSpecialObject(
                gold ? "mineral_gold" : "mineral_crystal");
            marker_kind = "mineral";
        } else if (scene_.id == "r_mercenary_visual") {
            if (active_mercenary_id_.empty()) {
                std::vector<const soulknight::game::items::SpecialObjectDefinition*> pool;
                for (const auto& object : item_assets_.SpecialObjects()) {
                    if (object.role == "mercenary" &&
                        object.id != "npc_trainer" &&
                        object.id != "npc_07_mad" &&
                        object.id != "npc_07") {
                        pool.push_back(&object);
                    }
                }
                if (!pool.empty()) chosen = pool[seed % pool.size()];
                marker_kind = "mercenary";
            } else if (mercenary_level_ < 5) {
                chosen = item_assets_.FindSpecialObject("npc_trainer");
                marker_kind = "mercenary_trainer";
            } else {
                chosen = item_assets_.FindSpecialObject("mineral_gold");
                marker_kind = "mineral";
            }
            if (chosen != nullptr && marker_kind == "mercenary") {
                marker_price = chosen->price;
            } else if (marker_kind == "mercenary_trainer") {
                marker_price = 10 * (1 << (mercenary_level_ - 1));
            }
        } else if (scene_.id == "r_mount_visual") {
            const bool creature = (seed % 10u) < 8u;
            chosen = item_assets_.FindSpecialObject(
                creature ? "npc_mount_creature" : "npc_mount_mech");
            marker_kind = "mount";
            marker_price = chosen != nullptr ? chosen->price : 0;
        }
        if (chosen != nullptr) {
            SpecialObjectInstance object;
            object.id = chosen->id;
            object.role = chosen->role;
            object.sprite = chosen->sprite;
            object.drop_item = chosen->drop_item;
            object.position = scene_.spawn + engine::Vec2{0.0f, 2.8f};
            object.price = marker_price > 0 ? marker_price : chosen->price;
            object.hp = chosen->hp;
            object.max_hp = chosen->hp;
            object.damage = chosen->damage;
            object.statue_index = chosen->statue_index;
            object.mineral_count = chosen->mineral_count;
            object.used = marker_kind == "statue" &&
                active_statue_id_ == object.id;
            special_objects_.push_back(object);
            if (!object.used) {
                engine::assets::InteractionMarker marker;
                marker.id = "special_" + object.id;
                marker.kind = marker_kind;
                marker.label = marker_kind == "mineral"
                    ? "MINERAL"
                    : object.id + (object.price > 0
                        ? "  -" + std::to_string(object.price) : "");
                marker.position = object.position;
                marker.radius = 2.4f;
                marker.price = object.price;
                marker.item_id = object.id;
                scene_.interaction_markers.push_back(std::move(marker));
            }
            LOG_INFO_APP("Special room: {} ({}) at ({:.2f},{:.2f})",
                         object.id, marker_kind, object.position.x,
                         object.position.y);
        }
    }
    if (!active_mercenary_id_.empty() && !mercenary_spawned_ &&
        item_assets_.IsLoaded()) {
        const auto* definition =
            item_assets_.FindSpecialObject(active_mercenary_id_);
        const int sprite = definition != nullptr
            ? scene_.FindSpriteIndex(definition->sprite) : -1;
        if (sprite >= 0) {
            engine::assets::WeaponSpecial special;
            special.sprite_index = sprite;
            special.duration = 100000.0f;
            special.damage = std::max(1, definition->damage);
            special.speed = 6.0f;
            special.range = 2.0f;
            special.interval = 1.0f;
            (void)summons_.Spawn(special, player_.Position(), 1, false);
            mercenary_spawned_ = true;
        }
    }

    const bool boss_room = scene_.id.rfind("r1_boss", 0) == 0 ||
                           scene_.id.rfind("r2_boss", 0) == 0 ||
                           scene_.id.rfind("r3_boss", 0) == 0;
    current_chest_definition_ = boss_room ? "chest_boss" : "chest01";
    const bool chest_room = scene_.id == "r_chest_visual";
    if (chest_room) {
        current_chest_definition_ = "chest_room_reward";
    }
    item_world_.EnterRoom(scene_.id, false,
                          scene_.spawn + engine::Vec2{0.0f, 2.4f},
                          current_chest_definition_);
    const bool chest_review = config_.review_mode &&
        config_.review_area == "chest" && dungeon;
    if (chest_review) {
        // Chest review row: show the three recovered types together.
        item_world_.AddChest("chest00", scene_.spawn + engine::Vec2{-4.0f, 2.0f});
        item_world_.AddChest("chest01", scene_.spawn + engine::Vec2{0.0f, 3.5f});
        item_world_.AddChest("chest02", scene_.spawn + engine::Vec2{4.0f, 2.0f});
        chest_spawned_rooms_.insert(scene_.id);
    } else if (dungeon && !chest_spawned_rooms_.contains(scene_.id) &&
               ((chest_room) ||
                (scene_.room.type == 1 &&
                 (cleared_rooms_.contains(scene_.id) ||
                  enemies_.AliveCount() == 0)))) {
        item_world_.AddChest(current_chest_definition_,
                             scene_.spawn + engine::Vec2{0.0f, 2.4f});
        chest_spawned_rooms_.insert(scene_.id);
    }
    // Recovered r_weapon_provider room: offer weapons from the level's
    // WeaponDropInfo pool. The room prefab itself contains a b_point; the
    // native test lays the offers on free floor around the room center.
    if (scene_.id == "r_weapon_provider_visual" &&
        !chest_spawned_rooms_.contains(scene_.id)) {
        int level = biome_.Current() != nullptr
            ? biome_.Current()->chest_level : 1;
        std::vector<const soulknight::game::items::WeaponDropEntry*> entries;
        for (const auto& drop : item_assets_.WeaponDrops()) {
            if (drop.level != level) continue;
            for (const auto& entry : drop.entries) entries.push_back(&entry);
        }
        if (!entries.empty()) {
            engine::RGRandomStream random;
            random.SetRandomSeed(static_cast<std::int32_t>(
                (static_cast<std::uint32_t>(map_seed_) ^ 0x57504E31u) &
                0x7FFFFFFFu));
            const engine::Vec2 offsets[] = {
                {-2.0f, 2.0f}, {0.0f, 2.4f}, {2.0f, 2.0f}};
            for (int offer = 0; offer < 3; ++offer) {
                const auto* entry = entries[static_cast<std::size_t>(
                    random.Range(0, static_cast<int>(entries.size())))];
                std::size_t index = weapons_.Count();
                for (std::size_t i = 0; i < weapons_.Count(); ++i) {
                    if (weapons_.Definitions()[i].id == entry->weapon) {
                        index = i;
                        break;
                    }
                }
                if (index >= weapons_.Count()) continue;
                SpawnWeaponPickupAt(index, scene_.spawn + offsets[offer]);
            }
            chest_spawned_rooms_.insert(scene_.id);
            LOG_INFO_APP("Weapon provider: offered 3 level-{} weapons", level);
        }
    }

    // The reference transfer_gate_02 is the actual portal object. The native
    // room scenes do not serialize a return gate, so place the recovered asset
    // at a deterministic clear floor position and expose the normal E prompt.
    // The Living Room material chest is the recovered static chest_material
    // prefab already present in HeroRoom. Do not overlay a generic chest03.
    if (dungeon && scene_.room.type == 2) {
        const bool has_portal = std::any_of(
            scene_.interaction_markers.begin(), scene_.interaction_markers.end(),
            [](const engine::assets::InteractionMarker& marker) {
                return marker.kind == "portal";
            });
        if (!has_portal) {
            engine::assets::InteractionMarker portal;
            portal.id = "return_portal";
            portal.kind = "portal";
            portal.label = "NEXT LEVEL";
            portal.position = scene_.spawn + engine::Vec2{4.5f, 3.0f};
            portal.radius = 3.2f;
            scene_.interaction_markers.push_back(std::move(portal));
        }
    }
}

void Application::OpenMechStation() {
    mech_station_open_ = true;
    mech_selected_ = 0;
    if (!active_mount_id_.empty()) {
        for (std::size_t i = 0; i < std::size(kMechOffers); ++i) {
            if (active_mount_id_ == kMechOffers[i].id) {
                mech_selected_ = static_cast<int>(i);
                break;
            }
        }
    }
    mech_status_.clear();
    PlayEffect("ui_click", 0.4f);
    LOG_INFO_APP("Mech station: opened (orbs={}, active={})",
                 player_orbs_,
                 active_mount_id_.empty() ? "none" : active_mount_id_);
}

void Application::CycleMechSelection(int delta) {
    if (!mech_station_open_) return;
    const int count = static_cast<int>(std::size(kMechOffers));
    mech_selected_ = (mech_selected_ + delta + count) % count;
    mech_status_.clear();
    PlayEffect("ui_click", 0.3f);
}

void Application::BuildMech() {
    if (!mech_station_open_) return;
    if (mech_selected_ < 0 ||
        static_cast<std::size_t>(mech_selected_) >= std::size(kMechOffers)) {
        return;
    }
    const MechOffer& offer = kMechOffers[mech_selected_];
    if (!active_mount_id_.empty()) {
        if (active_mount_id_ == offer.id) {
            active_mount_id_.clear();
            mount_speed_bonus_ = 0.0f;
            mech_status_ = "MECH STORED";
            PlayEffect("ui_click", 0.5f);
            LOG_INFO_APP("Mech station: stored {}", offer.id);
        } else {
            mech_status_ = "UNEQUIP CURRENT MECH";
        }
        return;
    }
    if (offer.unlock_key[0] != '\0') {
        mech_status_ = "BLUEPRINT REQUIRED";
        PlayEffect("ui_click", 0.35f);
        return;
    }
    if (player_orbs_ < offer.cost) {
        mech_status_ = "NEED " + std::to_string(offer.cost - player_orbs_) +
            " ORBS";
        PlayEffect("ui_click", 0.35f);
        return;
    }
    player_orbs_ -= offer.cost;
    active_mount_id_ = offer.id;
    mount_speed_bonus_ = offer.speed_rate;
    mech_status_ = "BUILT  " + std::string(offer.name);
    PlayEffect("weapon_fx_buy", 0.6f);
    ShowDialogue("ROBOTICS FACILITY", offer.name, 2.0f);
    LOG_INFO_APP("Mech station: built {} for {} orbs (remaining {}, "
                 "defence {} speedRate {:.2f})",
                 offer.id, offer.cost, player_orbs_, offer.defence,
                 offer.speed_rate);
}

void Application::SaveHubSuggestions() const {
    std::error_code error;
    const std::filesystem::path directory = project_root_ / "analysis";
    std::filesystem::create_directories(directory, error);
    const std::filesystem::path path = directory / "hub_suggestions.json";
    nlohmann::json document;
    document["schema"] = 1;
    document["build"] = "0.5.5-dev";
    document["area"] = "living-room";
    document["suggestions"] = nlohmann::json::array();
    for (const HubSuggestion& suggestion : hub_suggestions_) {
        nlohmann::json item;
        item["type"] = suggestion.type;
        item["position"] = {suggestion.position.x, suggestion.position.y};
        item["size"] = {suggestion.size.x, suggestion.size.y};
        item["note"] = suggestion.note;
        item["object_id"] = suggestion.object_id;
        item["timestamp"] = suggestion.timestamp;
        item["area"] = "living-room";
        document["suggestions"].push_back(std::move(item));
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        LOG_WARN_APP("Hub suggestions: cannot write {}", path.string());
        return;
    }
    stream << document.dump(2) << "\n";
    LOG_INFO_APP("Hub suggestions: wrote {} entr{} to {}",
                 hub_suggestions_.size(),
                 hub_suggestions_.size() == 1 ? "y" : "ies",
                 path.string());
}

void Application::LoadHubSuggestions() {
    hub_suggestions_.clear();
    const std::filesystem::path path =
        project_root_ / "analysis" / "hub_suggestions.json";
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return;
    nlohmann::json document = nlohmann::json::parse(
        stream, nullptr, false);
    if (document.is_discarded() || !document.is_object() ||
        !document.contains("suggestions") ||
        !document["suggestions"].is_array()) {
        LOG_WARN_APP("Hub suggestions: malformed {}", path.string());
        return;
    }
    for (const auto& item : document["suggestions"]) {
        if (!item.is_object()) continue;
        HubSuggestion suggestion;
        suggestion.type = item.value("type", std::string("note"));
        if (item.contains("position") && item["position"].is_array() &&
            item["position"].size() == 2) {
            suggestion.position = {item["position"][0].get<float>(),
                                   item["position"][1].get<float>()};
        }
        if (item.contains("size") && item["size"].is_array() &&
            item["size"].size() == 2) {
            suggestion.size = {item["size"][0].get<float>(),
                               item["size"][1].get<float>()};
        }
        suggestion.note = item.value("note", std::string{});
        suggestion.object_id = item.value("object_id", std::string{});
        suggestion.timestamp = item.value("timestamp", 0.0);
        hub_suggestions_.push_back(std::move(suggestion));
    }
    LOG_INFO_APP("Hub suggestions: loaded {} from {}",
                 hub_suggestions_.size(), path.string());
}

bool Application::HandleHubSuggestionEvent(const SDL_Event& event) {
    if (!living_room_suggestions_ || !scene_.is_hub ||
        !suggestion_edit_mode_) {
        return false;
    }
    if (event.type == SDL_EVENT_TEXT_INPUT) {
        if (suggestion_note_entry_) {
            suggestion_note_buffer_ += event.text.text;
            return true;
        }
        return false;
    }
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        const SDL_Scancode code = event.key.scancode;
        const bool ctrl =
            (event.key.mod & SDL_KMOD_CTRL) != 0;
        if (suggestion_note_entry_) {
            if (code == SDL_SCANCODE_BACKSPACE &&
                !suggestion_note_buffer_.empty()) {
                suggestion_note_buffer_.pop_back();
            } else if (code == SDL_SCANCODE_ESCAPE) {
                suggestion_note_entry_ = false;
                suggestion_note_buffer_.clear();
                suggestion_note_index_ = -1;
                SDL_StopTextInput(window_.GetSDLWindow());
            } else if (code == SDL_SCANCODE_RETURN ||
                       code == SDL_SCANCODE_KP_ENTER) {
                if (suggestion_note_index_ >= 0 &&
                    static_cast<std::size_t>(suggestion_note_index_) <
                        hub_suggestions_.size()) {
                    hub_suggestions_[static_cast<std::size_t>(
                        suggestion_note_index_)].note =
                        suggestion_note_buffer_;
                    SaveHubSuggestions();
                }
                suggestion_note_entry_ = false;
                suggestion_note_buffer_.clear();
                suggestion_note_index_ = -1;
                SDL_StopTextInput(window_.GetSDLWindow());
            }
            return true;
        }
        if (code == SDL_SCANCODE_ESCAPE) {
            suggestion_edit_mode_ = false;
            suggestion_dragging_ = false;
            PlayEffect("ui_click", 0.3f);
            return true;
        }
        if (code == SDL_SCANCODE_T) {
            suggestion_type_index_ = (suggestion_type_index_ + 1) %
                static_cast<int>(std::size(kSuggestionTypes));
            LOG_INFO_APP("Hub suggestion type: {}",
                         kSuggestionTypes[suggestion_type_index_]);
            return true;
        }
        if (code == SDL_SCANCODE_L) {
            suggestion_list_visible_ = !suggestion_list_visible_;
            return true;
        }
        if (code == SDL_SCANCODE_BACKSPACE ||
            code == SDL_SCANCODE_DELETE) {
            if (!hub_suggestions_.empty()) {
                hub_suggestions_.pop_back();
                SaveHubSuggestions();
            }
            return true;
        }
        if (ctrl && code == SDL_SCANCODE_X) {
            hub_suggestions_.clear();
            SaveHubSuggestions();
            LOG_INFO_APP("Hub suggestions: cleared");
            return true;
        }
        if (ctrl && code == SDL_SCANCODE_E) {
            SaveHubSuggestions();
            return true;
        }
        return true;
    }
    if (event.type != SDL_EVENT_MOUSE_MOTION &&
        event.type != SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event.type != SDL_EVENT_MOUSE_BUTTON_UP) {
        return false;
    }
    const engine::Vec2 logical = logical_viewport_.WindowPointToLogical(
        {event.type == SDL_EVENT_MOUSE_MOTION
             ? event.motion.x : event.button.x,
         event.type == SDL_EVENT_MOUSE_MOTION
             ? event.motion.y : event.button.y});
    last_mouse_logical_ = logical;
    last_mouse_world_ = render_camera_.ScreenToWorld(logical);
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event.button.button == SDL_BUTTON_LEFT) {
        suggestion_dragging_ = true;
        suggestion_drag_start_ = last_mouse_world_;
        return true;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
        event.button.button == SDL_BUTTON_LEFT && suggestion_dragging_) {
        suggestion_dragging_ = false;
        HubSuggestion suggestion;
        suggestion.type = kSuggestionTypes[suggestion_type_index_];
        const engine::Vec2 min{
            std::min(suggestion_drag_start_.x, last_mouse_world_.x),
            std::min(suggestion_drag_start_.y, last_mouse_world_.y)};
        const engine::Vec2 max{
            std::max(suggestion_drag_start_.x, last_mouse_world_.x),
            std::max(suggestion_drag_start_.y, last_mouse_world_.y)};
        suggestion.position = min;
        suggestion.size = {std::max(0.5f, max.x - min.x),
                           std::max(0.5f, max.y - min.y)};
        suggestion.timestamp = UnixSeconds();
        hub_suggestions_.push_back(std::move(suggestion));
        SaveHubSuggestions();
        if (std::string(kSuggestionTypes[suggestion_type_index_]) == "note") {
            suggestion_note_entry_ = true;
            suggestion_note_buffer_.clear();
            suggestion_note_index_ =
                static_cast<int>(hub_suggestions_.size()) - 1;
            SDL_StartTextInput(window_.GetSDLWindow());
        }
        PlayEffect("pickup", 0.35f);
        return true;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event.button.button == SDL_BUTTON_RIGHT) {
        int nearest = -1;
        float best = 1.5f * 1.5f;
        for (std::size_t i = 0; i < hub_suggestions_.size(); ++i) {
            const engine::Vec2 center = hub_suggestions_[i].position +
                hub_suggestions_[i].size * 0.5f;
            const float distance =
                (center - last_mouse_world_).LengthSquared();
            if (distance <= best) {
                best = distance;
                nearest = static_cast<int>(i);
            }
        }
        if (nearest >= 0) {
            hub_suggestions_.erase(hub_suggestions_.begin() + nearest);
            SaveHubSuggestions();
            PlayEffect("ui_click", 0.3f);
        }
        return true;
    }
    return true;
}

void Application::DrawHubSuggestionOverlay() {
    if (!living_room_suggestions_ || !scene_.is_hub) return;
    auto& text = dev_ui_.Text();
    const float half_w = camera_.HalfWidth();
    const float half_h = camera_.HalfHeight();
    const engine::Vec2 origin{camera_.position.x - half_w + 0.35f,
                              camera_.position.y + half_h - 4.2f};
    if (!suggestion_edit_mode_) {
        text.DrawText(sprite_batch_, "QA: G SUGGESTIONS",
                      origin, 0.0065f, {170, 205, 235, 220}, 1505);
        return;
    }
    const float scale = 0.0065f;
    text.DrawPanel(sprite_batch_,
                   origin + engine::Vec2{0.0f, 0.15f},
                   {5.4f, 0.5f}, {15, 20, 30, 210}, 1504);
    const std::string header =
        std::string("QA MARK MODE  TYPE: ") +
        kSuggestionTypes[suggestion_type_index_] +
        "  [T] NEXT  [L] LIST  RMB DELETE  [ESC] EXIT";
    text.DrawText(sprite_batch_, header, origin, scale,
                  {255, 220, 145, 255}, 1505);
    if (suggestion_note_entry_) {
        text.DrawText(sprite_batch_,
                      "NOTE: " + suggestion_note_buffer_ +
                          "   [ENTER] SAVE  [ESC] CANCEL",
                      origin + engine::Vec2{0.0f, -0.42f}, scale,
                      {150, 235, 185, 255}, 1505);
    }
    // Crosshair at the exact cursor world position.
    text.DrawLine(sprite_batch_, last_mouse_world_ + engine::Vec2{-0.35f, 0},
                  last_mouse_world_ + engine::Vec2{0.35f, 0}, 0.03f,
                  {255, 220, 80, 235}, 1506);
    text.DrawLine(sprite_batch_, last_mouse_world_ + engine::Vec2{0, -0.35f},
                  last_mouse_world_ + engine::Vec2{0, 0.35f}, 0.03f,
                  {255, 220, 80, 235}, 1506);
    for (const HubSuggestion& suggestion : hub_suggestions_) {
        const engine::Color color = suggestion.type == "wall"
            ? engine::Color{255, 145, 90, 210}
            : suggestion.type == "collision" ||
                      suggestion.type == "clipping"
            ? engine::Color{255, 80, 80, 210}
            : suggestion.type == "depth"
            ? engine::Color{160, 130, 255, 210}
            : engine::Color{90, 220, 180, 210};
        text.DrawPanelBorder(sprite_batch_, suggestion.position,
                             suggestion.size, {color.r, color.g, color.b, 35},
                             color, 0.05f, 1505);
    }
    if (suggestion_list_visible_) {
        engine::Vec2 list_origin{camera_.position.x + half_w - 12.6f,
                                 camera_.position.y + half_h - 5.0f};
        text.DrawPanelBorder(sprite_batch_, list_origin,
                             {5.4f, 3.8f}, {15, 20, 30, 235},
                             {188, 150, 74, 255}, 0.03f, 1506);
        text.DrawText(sprite_batch_,
                      "SUGGESTIONS  " +
                          std::to_string(hub_suggestions_.size()),
                      list_origin + engine::Vec2{0.2f, -0.35f}, scale,
                      {255, 220, 145, 255}, 1507);
        for (std::size_t i = 0; i < hub_suggestions_.size() && i < 7; ++i) {
            const auto& suggestion = hub_suggestions_[i];
            char line[160];
            std::snprintf(line, sizeof(line), "%zu  %-12s (%.1f, %.1f) %s",
                          i + 1, suggestion.type.c_str(),
                          suggestion.position.x, suggestion.position.y,
                          suggestion.note.c_str());
            text.DrawText(sprite_batch_, line,
                          list_origin + engine::Vec2{0.2f,
                              -0.8f - static_cast<float>(i) * 0.42f},
                          scale, {205, 220, 235, 255}, 1507);
        }
    }
}

void Application::DrawCalibrationOverlay() {
    auto& text = dev_ui_.Text();
    const float half_w = camera_.HalfWidth();
    const float half_h = camera_.HalfHeight();
    // Cover the normal world completely; this is a development-only view.
    text.DrawPanel(sprite_batch_,
                   {camera_.position.x - half_w,
                    camera_.position.y + half_h},
                   {half_w * 2.0f, half_h * 2.0f}, {4, 6, 10, 248}, 1450);
    struct RoomSpec { const char* label; int width; int height; };
    const RoomSpec specs[] = {
        {"11x11", 11, 11}, {"15x15", 15, 15}, {"15x21", 15, 21},
        {"21x15", 21, 15}, {"21x21", 21, 21}};
    const float cell = std::min(0.30f,
        (half_w * 1.78f) / 103.0f);
    const float gap = cell * 1.6f;
    float total_units = 0.0f;
    for (const RoomSpec& spec : specs) {
        total_units += static_cast<float>(spec.width);
    }
    const float total = total_units * cell + gap * 4.0f;
    float cursor = camera_.position.x - total * 0.5f;
    const float top = camera_.position.y + half_h * 0.58f;
    const int32_t player_sprite = player_render_.body_instance >= 0
        ? static_cast<int32_t>(scene_.instances[static_cast<std::size_t>(
              player_render_.body_instance)].sprite_index) : -1;
    int32_t enemy_sprite = -1;
    if (!scene_.enemy_definitions.empty() &&
        scene_.enemy_definitions.front().body_sprite_index >= 0) {
        enemy_sprite = scene_.enemy_definitions.front().body_sprite_index;
    }
    int32_t chest_sprite = -1;
    if (item_assets_.IsLoaded()) {
        if (const auto* chest = item_assets_.FindChest("chest01");
            chest != nullptr && !chest->visuals.empty()) {
            chest_sprite = scene_.FindSpriteIndex(chest->visuals[0].sprite);
        }
    }
    const auto draw_sprite = [&](int32_t index, const engine::Vec2& at,
                                 float scale) {
        if (index < 0 ||
            static_cast<std::size_t>(index) >= scene_.sprites.size()) {
            return;
        }
        const auto& sprite = scene_.sprites[static_cast<std::size_t>(index)];
        engine::rendering::SpriteDraw draw;
        draw.position = at;
        draw.size = sprite.WorldSize() * scale;
        draw.uv = sprite.UvRect();
        draw.color = engine::Color::White();
        draw.layer = engine::rendering::SortLayer::Character;
        draw.sorting_order = 200000;
        draw.texture_id = sprite.texture_id;
        draw.pivot = sprite.pivot;
        (void)sprite_batch_.DrawSprite(draw);
    };
    for (const RoomSpec& spec : specs) {
        const float width = static_cast<float>(spec.width) * cell;
        const float height = static_cast<float>(spec.height) * cell;
        const engine::Vec2 origin{cursor, top};
        const float edge = 0.03f;
        const engine::Color outline{220, 225, 235, 255};
        text.DrawPanel(sprite_batch_, origin,
                       {width, edge}, outline, 1452);
        text.DrawPanel(sprite_batch_, {origin.x, origin.y - height},
                       {width, edge}, outline, 1452);
        text.DrawPanel(sprite_batch_, origin,
                       {edge, height}, outline, 1452);
        text.DrawPanel(sprite_batch_, {origin.x + width - edge, origin.y},
                       {edge, height}, outline, 1452);
        for (int x = 1; x < spec.width; ++x) {
            const float gx = origin.x + static_cast<float>(x) * cell;
            text.DrawLine(sprite_batch_, {gx, origin.y},
                          {gx, origin.y - height}, 0.012f,
                          {65, 75, 90, 190}, 1452);
        }
        for (int y = 1; y < spec.height; ++y) {
            const float gy = origin.y - static_cast<float>(y) * cell;
            text.DrawLine(sprite_batch_, {origin.x, gy},
                          {origin.x + width, gy}, 0.012f,
                          {65, 75, 90, 190}, 1452);
        }
        text.DrawText(sprite_batch_, spec.label,
                      {origin.x, origin.y + 0.55f}, 0.009f,
                      {255, 220, 145, 255}, 1454);
        char measurement[96];
        std::snprintf(measurement, sizeof(measurement),
                      "%dx%d units  cell=%.2f", spec.width, spec.height,
                      cell);
        text.DrawText(sprite_batch_, measurement,
                      {origin.x, origin.y + 0.30f}, 0.0056f,
                      {190, 205, 220, 255}, 1454);
        draw_sprite(player_sprite,
                    {origin.x + width * 0.50f, origin.y - height * 0.56f},
                    0.72f);
        draw_sprite(enemy_sprite,
                    {origin.x + width * 0.72f, origin.y - height * 0.56f},
                    0.72f);
        draw_sprite(chest_sprite,
                    {origin.x + width * 0.28f, origin.y - height * 0.48f},
                    0.55f);
        // One wall segment (north), one door (south) and the room outline.
        text.DrawPanel(sprite_batch_,
                       {origin.x + cell, origin.y - cell},
                       {width - cell * 2.0f, cell * 0.30f},
                       {95, 105, 120, 255}, 1453);
        text.DrawPanel(sprite_batch_,
                       {origin.x + width * 0.5f - cell, origin.y - height},
                       {cell * 2.0f, cell * 0.24f},
                       {155, 120, 70, 255}, 1453);
        cursor += width + gap;
    }
    text.DrawText(sprite_batch_,
        "CALIBRATION  player collider 0.80x0.80  sprite 1.62x1.87  "
        "camera ortho " + std::to_string(camera_.orthographic_size),
        {camera_.position.x - half_w + 0.3f,
         camera_.position.y - half_h + 0.3f},
        0.0075f, {225, 235, 245, 255}, 1455);
}

void Application::ConfigureHubGarden() {
    if (!scene_.is_hub) return;
    if (!plant_system_.Plots().empty()) {
        // Returning to the Living Room must not reset growing plots.
        if (selected_plant_seed_.empty()) {
            selected_plant_seed_ = plant_system_.FirstAvailableSeed();
        }
        return;
    }
    plant_system_.Clear();
    // Recovered land_root positions and serialized lock sprites. Growth uses
    // the documented native reconstruction timer (30 s) because the
    // authoritative PlantInfo/Resources table is absent from the APK assets.
    for (const engine::assets::GardenPlot& plot : scene_.garden_plots) {
        plant_system_.AddPlot(plot.id, plot.position, 30.0f, plot.locked, 4);
    }
    if (plant_system_.Seeds().empty()) {
        // Starter carrot seed: the seed economy is not present in the
        // extracted save data, so one recovered seed is available for the
        // native planting loop. Everything grown/harvested is real prefab data.
        plant_system_.AddSeed("plant_carrot", 4);
    }
    selected_plant_seed_ = plant_system_.FirstAvailableSeed();
    LOG_INFO_APP("Garden: {} recovered plots ({} unlocked), selected seed={}",
                 plant_system_.Plots().size(),
                 static_cast<int>(std::count_if(
                     plant_system_.Plots().begin(), plant_system_.Plots().end(),
                     [](const auto& plot) { return !plot.locked; })),
                 selected_plant_seed_);
}

void Application::ConfigureCameraBounds() {
    camera_bounds_valid_ = false;
    if (scene_.interior_bounds.w > 0.0f && scene_.interior_bounds.h > 0.0f) {
        camera_bounds_ = {
            {scene_.interior_bounds.x, scene_.interior_bounds.y},
            {scene_.interior_bounds.x + scene_.interior_bounds.w,
             scene_.interior_bounds.y + scene_.interior_bounds.h}};
        camera_bounds_valid_ = true;
        return;
    }
    float min_x = 1e9f;
    float min_y = 1e9f;
    float max_x = -1e9f;
    float max_y = -1e9f;
    for (const engine::assets::SpriteInstance& instance : scene_.instances) {
        if (instance.path.find("room_tilemap/floor_") == std::string::npos) {
            continue;
        }
        min_x = std::min(min_x, instance.position.x - 0.5f);
        min_y = std::min(min_y, instance.position.y - 0.5f);
        max_x = std::max(max_x, instance.position.x + 0.5f);
        max_y = std::max(max_y, instance.position.y + 0.5f);
    }
    if (max_x > min_x && max_y > min_y) {
        camera_bounds_ = {{min_x, min_y}, {max_x, max_y}};
        camera_bounds_valid_ = true;
        return;
    }
    if (!scene_.floor_cells.empty()) {
        float cell_min_x = 1e9f;
        float cell_min_y = 1e9f;
        float cell_max_x = -1e9f;
        float cell_max_y = -1e9f;
        for (const engine::Vec2& cell : scene_.floor_cells) {
            cell_min_x = std::min(cell_min_x, cell.x);
            cell_min_y = std::min(cell_min_y, cell.y);
            cell_max_x = std::max(cell_max_x, cell.x + 1.0f);
            cell_max_y = std::max(cell_max_y, cell.y + 1.0f);
        }
        if (cell_max_x > cell_min_x && cell_max_y > cell_min_y) {
            camera_bounds_ = {{cell_min_x, cell_min_y},
                              {cell_max_x, cell_max_y}};
            camera_bounds_valid_ = true;
        }
    }
}

void Application::ShowDialogue(const std::string& speaker,
                               const std::string& text, float seconds) {
    dialogue_speaker_ = speaker;
    dialogue_text_ = text;
    dialogue_timer_ = std::max(0.1f, seconds);
}

void Application::TryInteract() {
    const auto* marker = NearbyInteraction();
    if (marker == nullptr) {
        if (NearOpenDoor()) SwitchRoom();
        return;
    }
    if (marker->kind == "npc") {
        // Recovered TalkCharNpc/TalkTrigger lines. Every CharacterN_talk*
        // localization term is wired; the conversation advances on each
        // deliberate interaction rather than auto-chattering.
        std::vector<std::string> lines = marker->talk_lines;
        if (lines.empty()) {
            if (!marker->text.empty()) lines.push_back(marker->text);
            if (!marker->text_alt.empty()) lines.push_back(marker->text_alt);
        }
        if (lines.empty()) {
            ShowDialogue(marker->speaker, "...", 1.5f);
            return;
        }
        const std::size_t line_index =
            npc_talk_line_[marker->id]++ % lines.size();
        ShowDialogue(marker->speaker, lines[line_index], 2.5f);
        PlayEffect("pickup", 0.55f);
        LOG_INFO_APP("NPC talk: {} line {}/{} ({})", marker->speaker,
                     line_index + 1, lines.size(), marker->dialogue_term);
    } else if (marker->kind == "weapon_chest") {
        if (item_world_.OpenNearestChest(marker->position, 3.2f)) {
            ShowDialogue("", "WEAPON CHEST OPENED", 1.5f);
            PlayEffect("chest_open", 0.6f);
            LOG_INFO_APP("Living Room: weapon chest opened");
        } else {
            ShowDialogue("", "CHEST ALREADY OPEN", 1.2f);
        }
    } else if (marker->kind == "mission") {
        const auto& lines = marker->talk_lines;
        if (lines.empty()) {
            ShowDialogue(marker->label, "NO TASKS RECOVERED", 1.5f);
            return;
        }
        const std::size_t line_index =
            npc_talk_line_[marker->id]++ % lines.size();
        ShowDialogue("TASK BOARD", lines[line_index], 2.5f);
        PlayEffect("ui_click", 0.4f);
        LOG_INFO_APP("Mission board: task {}/{}", line_index + 1, lines.size());
    } else if (marker->kind == "pet") {
        std::vector<std::string> names;
        std::vector<int> sprites;
        for (const auto& definition : pet_.Definitions()) {
            names.push_back(definition.display_name);
            sprites.push_back(definition.body_sprite_index);
        }
        game_ui_.OpenPetSelect(static_cast<int>(pet_.SelectedIndex()),
                               static_cast<int>(names.size()));
        LOG_INFO_APP("Pet menu: {} recovered pet(s)", names.size());
    } else if (marker->kind == "character") {
        game_ui_.OpenCharacterSelect(
            static_cast<int>(player_characters_.ActiveIndex()));
        LOG_INFO_APP("Living Room: character station opened");
    } else if (marker->kind == "forge") {
        game_ui_.OpenWorkshop(0, static_cast<int>(workshop_.Recipes().size()));
        workshop_status_.clear();
        LOG_INFO_APP("Living Room: workshop opened ({} recipes)",
                     workshop_.Recipes().size());
    } else if (marker->kind == "weapon_station") {
        game_ui_.OpenWorkshop(0, static_cast<int>(workshop_.Recipes().size()));
        workshop_status_ = "WEAPON STATION - BLUEPRINTS AND MATERIALS";
        LOG_INFO_APP("Living Room: weapon station blueprint crafting opened "
                     "({} recipes)", workshop_.Recipes().size());
    } else if (marker->kind == "material_chest") {
        game_ui_.OpenMaterialChest();
        LOG_INFO_APP("Living Room: material storage opened ({} stacks)",
                     workshop_.SnapshotMaterials().size());
    } else if (marker->kind == "garden") {
        const int plot_index =
            plant_system_.NearestPlot(player_.Position(), 5.0f);
        if (plot_index < 0) {
            ShowDialogue("GARDEN", "WALK TO A RECOVERED PLOT", 1.5f);
            return;
        }
        auto& plot = const_cast<game::hub::PlantPlot&>(
            plant_system_.Plots()[static_cast<std::size_t>(plot_index)]);
        if (plot.locked) {
            ShowDialogue("GARDEN", "PLOT LOCKED", 1.5f);
            return;
        }
        if (plot.state == game::hub::PlantGrowthState::Empty) {
            std::string seed = selected_plant_seed_.empty()
                ? plant_system_.FirstAvailableSeed()
                : selected_plant_seed_;
            if (seed.empty()) {
                ShowDialogue("GARDEN", "NO SEEDS", 1.5f);
                return;
            }
            const auto* definition = item_assets_.FindPlant(seed);
            if (definition == nullptr || definition->states.empty()) {
                ShowDialogue("GARDEN", "SEED DATA UNAVAILABLE", 1.5f);
                return;
            }
            const auto result = plant_system_.Interact(
                plot_index, seed, definition->growth_seconds,
                static_cast<int>(definition->states.size()),
                definition->item_id, definition->harvest_count);
            if (result == game::hub::PlantInteraction::Planted) {
                PlayEffect("pickup", 0.55f);
                ShowDialogue("GARDEN",
                             definition->display_name + " PLANTED", 1.5f);
                LOG_INFO_APP("Garden: planted {} in {} ({:.0f}s)",
                             seed, plot.id, definition->growth_seconds);
            } else if (result == game::hub::PlantInteraction::NoSeed) {
                ShowDialogue("GARDEN", "NO SEEDS", 1.5f);
            }
            return;
        }
        if (plot.state == game::hub::PlantGrowthState::Growing) {
            const auto result = plant_system_.Interact(plot_index, {});
            if (result == game::hub::PlantInteraction::Watered) {
                PlayEffect("pickup", 0.45f);
                ShowDialogue("GARDEN", "WATERED", 1.2f);
                LOG_INFO_APP("Garden: watered {}", plot.id);
            } else {
                const int remaining = std::max(
                    0, static_cast<int>(std::ceil(
                        plot.growth_time - plot.elapsed)));
                ShowDialogue("GARDEN",
                             "GROWING  " + std::to_string(remaining) + "S",
                             1.2f);
            }
            return;
        }
        const std::string species = plot.species;
        const std::string harvest_item = plot.harvest_item;
        const int yield_count = plot.yield_count;
        const auto result = plant_system_.Interact(plot_index, {});
        if (result != game::hub::PlantInteraction::Harvested) return;
        const auto* definition = item_assets_.FindPlant(species);
        const std::string display_name =
            definition != nullptr ? definition->display_name : species;
        if (harvest_item.rfind("weapon_", 0) == 0) {
            std::size_t weapon_index = weapons_.Count();
            for (std::size_t i = 0; i < weapons_.Count(); ++i) {
                if (weapons_.Definitions()[i].id == harvest_item) {
                    weapon_index = i;
                    break;
                }
            }
            if (weapon_index < weapons_.Count()) {
                SpawnWeaponPickupAt(weapon_index,
                                    player_.Position() + engine::Vec2{0.0f, 1.2f});
                ShowDialogue("GARDEN", display_name + " HARVESTED", 1.5f);
            } else {
                ShowDialogue("GARDEN", "WEAPON HARVESTED", 1.5f);
            }
        } else if (harvest_item.rfind("material_", 0) == 0) {
            workshop_.AddMaterial(harvest_item, std::max(1, yield_count));
            ShowDialogue("GARDEN",
                         "+" + std::to_string(std::max(1, yield_count)) +
                         " " + display_name, 1.5f);
        } else {
            ShowDialogue("GARDEN", display_name + " HARVESTED", 1.5f);
        }
        PlayEffect("pickup", 0.65f);
        LOG_INFO_APP("Garden: harvested {} from {} (total {})", species,
                     plot.id, plant_system_.HarvestedCount());
    } else if (marker->kind == "mech") {
        OpenMechStation();
    } else if (marker->kind == "statue") {
        const auto object = std::find_if(
            special_objects_.begin(), special_objects_.end(),
            [&](const SpecialObjectInstance& candidate) {
                return candidate.id == marker->item_id;
            });
        if (object == special_objects_.end() || object->used) {
            ShowDialogue("", "STATUE ALREADY USED", 1.5f);
            return;
        }
        const double now = NowSeconds();
        if (statue_confirm_id_ != object->id ||
            statue_confirm_seconds_ < 0.0 ||
            now - statue_confirm_seconds_ > 4.0) {
            statue_confirm_id_ = object->id;
            statue_confirm_seconds_ = now;
            ShowDialogue("STATUE", "USE FOR " +
                         std::to_string(object->price) +
                         " COINS? PRESS E AGAIN", 2.0f);
            PlayEffect("ui_click", 0.4f);
            return;
        }
        if (player_coins_ < object->price) {
            ShowDialogue("", "NOT ENOUGH COINS", 1.5f);
            return;
        }
        player_coins_ -= object->price;
        active_statue_id_ = object->id;
        object->used = true;
        statue_confirm_id_.clear();
        statue_confirm_seconds_ = -1.0;
        PlayEffect("weapon_fx_buy", 0.65f);
        ShowDialogue("STATUE", "BLESSING RECEIVED", 2.0f);
        LOG_INFO_APP("Statue: purchased {} (index {}) for {}", object->id,
                     object->statue_index, object->price);
    } else if (marker->kind == "mineral") {
        const auto object = std::find_if(
            special_objects_.begin(), special_objects_.end(),
            [&](const SpecialObjectInstance& candidate) {
                return candidate.id == marker->item_id;
            });
        if (object == special_objects_.end() || !object->active) return;
        object->hp = std::max(0, object->hp - 10);
        PlayEffect("enemy_hit", 0.35f);
        if (object->hp > 0) {
            ShowDialogue("MINERAL", std::to_string(object->hp) + " HP", 1.0f);
            return;
        }
        object->active = false;
        const int count = std::max(1, object->mineral_count);
        if (object->drop_item == "coin_2") {
            item_world_.SpawnCoins("coin_2", count, object->position);
        } else {
            for (int i = 0; i < count; ++i) {
                boxes_.SpawnEnergyAt(object->position +
                    engine::Vec2{static_cast<float>(i % 5) * 0.12f - 0.3f,
                                 0.3f + static_cast<float>(i / 5) * 0.12f});
            }
        }
        PlayEffect("pickup", 0.6f);
        ShowDialogue("MINERAL", "MINED", 1.5f);
        LOG_INFO_APP("Mineral: {} destroyed, {} drops", object->id, count);
    } else if (marker->kind == "mercenary" ||
               marker->kind == "mercenary_trainer") {
        if (marker->kind == "mercenary_trainer") {
            const int price = 10 * (1 << std::max(0, mercenary_level_ - 1));
            if (player_coins_ < price) {
                ShowDialogue("", "NOT ENOUGH COINS", 1.5f);
                return;
            }
            player_coins_ -= price;
            ++mercenary_level_;
            PlayEffect("weapon_fx_buy", 0.6f);
            ShowDialogue("TRAINER", "MERCENARY LEVEL " +
                         std::to_string(mercenary_level_), 2.0f);
            LOG_INFO_APP("Mercenary trained to level {} for {}",
                         mercenary_level_, price);
            return;
        }
        const auto object = std::find_if(
            special_objects_.begin(), special_objects_.end(),
            [&](const SpecialObjectInstance& candidate) {
                return candidate.id == marker->item_id;
            });
        if (object == special_objects_.end() || object->used) return;
        if (player_coins_ < object->price) {
            ShowDialogue("", "NOT ENOUGH COINS", 1.5f);
            return;
        }
        player_coins_ -= object->price;
        active_mercenary_id_ = object->id;
        mercenary_level_ = 1;
        mercenary_spawned_ = false;
        object->used = true;
        PlayEffect("weapon_fx_buy", 0.6f);
        ShowDialogue("MERCENARY", "RECRUITED", 2.0f);
        LOG_INFO_APP("Mercenary: recruited {} for {}", object->id,
                     object->price);
    } else if (marker->kind == "mount") {
        const auto object = std::find_if(
            special_objects_.begin(), special_objects_.end(),
            [&](const SpecialObjectInstance& candidate) {
                return candidate.id == marker->item_id;
            });
        if (object == special_objects_.end()) return;
        if (active_mount_id_ == object->id) {
            active_mount_id_.clear();
            mount_speed_bonus_ = 0.0f;
            ShowDialogue("MOUNT", "DISMOUNTED", 1.5f);
            PlayEffect("ui_click", 0.4f);
            LOG_INFO_APP("Mount: dismounted {}", object->id);
            return;
        }
        if (!active_mount_id_.empty()) {
            ShowDialogue("", "ALREADY MOUNTED", 1.5f);
            return;
        }
        if (player_coins_ < object->price) {
            ShowDialogue("", "NOT ENOUGH COINS", 1.5f);
            return;
        }
        player_coins_ -= object->price;
        active_mount_id_ = object->id;
        mount_speed_bonus_ = 0.35f;
        PlayEffect("weapon_fx_buy", 0.6f);
        ShowDialogue("MOUNT", "MOUNTED", 2.0f);
        LOG_INFO_APP("Mount: purchased {} for {}", object->id,
                     object->price);
    } else if (marker->kind == "seller") {
        const std::size_t index = marker->seller_index >= 0
            ? static_cast<std::size_t>(marker->seller_index) : 0;
        const auto* offer = seller_.FindByIndex(index);
        if (offer == nullptr || offer->sold) {
            ShowDialogue("", "SOLD OUT", 1.2f);
            return;
        }
        const std::string purchased = offer->display_name;
        const std::string purchased_weapon = offer->weapon_id;
        const engine::Vec2 purchased_position = offer->position;
        const soulknight::game::items::SellerPurchase result = seller_.Purchase(
            index, player_coins_, player_health_.hp, player_health_.max_hp,
            player_energy_.energy, player_energy_.max_energy);
        if (result == soulknight::game::items::SellerPurchase::NoMoney) {
            ShowDialogue("", "NOT ENOUGH COINS", 1.5f);
            PlayEffect("ui_click", 0.4f);
            LOG_INFO_APP("Seller: insufficient coins for {} ({})",
                         purchased, marker->price);
            return;
        }
        if (result != soulknight::game::items::SellerPurchase::Success) return;
        sold_shop_items_.insert(marker->item_id);
        PlayEffect("weapon_fx_buy", 0.65f);
        if (!purchased_weapon.empty()) {
            std::size_t weapon_index = weapons_.Count();
            for (std::size_t i = 0; i < weapons_.Count(); ++i) {
                if (weapons_.Definitions()[i].id == purchased_weapon) {
                    weapon_index = i;
                    break;
                }
            }
            if (weapon_index < weapons_.Count()) {
                SpawnWeaponPickupAt(
                    weapon_index, purchased_position + engine::Vec2{0.0f, 0.9f});
            }
        }
        ShowDialogue("SELLER", "BOUGHT " + purchased + "  -" +
                     std::to_string(marker->price), 2.0f);
        LOG_INFO_APP("Seller: bought {} for {} coins", purchased,
                     marker->price);
    } else if (marker->kind == "trader") {
        game_ui_.OpenTrader();
        LOG_INFO_APP("Trader: opened orb weapon inventory");
        PlayUiSound("ui_click");
    } else if (marker->kind == "portal") {
        if (enemies_.AliveCount() > 0 &&
            !cleared_rooms_.contains(scene_.id)) {
            ShowDialogue("", "DEFEAT ALL ENEMIES FIRST", 1.5f);
            return;
        }
        const double now = NowSeconds();
        if (return_portal_confirm_seconds_ >= 0.0 &&
            now - return_portal_confirm_seconds_ <= 3.0) {
            CaptureCurrentRoomState();
            return_portal_confirm_seconds_ = -1.0;
            if (scene_.room.type == 2) {
                if (AdvanceDungeonLevel()) {
                    LOG_INFO_APP("Portal: advanced to {}",
                                 biome_.Current()->id);
                } else {
                    ShowDialogue("", "CAMPAIGN COMPLETE", 3.0f);
                    LOG_INFO_APP("Portal: no further recovered level");
                }
            } else if (ActivateRoom(world_hub_id_, "")) {
                LOG_INFO_APP("Portal: returned to Living Room");
            }
        } else {
            return_portal_confirm_seconds_ = now;
            LOG_INFO_APP("Portal: E again within 3 s to confirm");
        }
    } else if (marker->kind == "door") {
        SwitchRoom();
    }
}

bool Application::TryPickupWeapon() {
    std::size_t weapon_index = 0;
    engine::Vec2 pickup_position = player_.Position();
    if (!weapon_pickups_.TakeNearest(player_.Position(), 1.5f,
                                     &weapon_index, &pickup_position)) {
        return false;
    }
    // HYPOTHESIS (pickup replacement): the original inventory UI is not yet
    // recovered. The native foundation swaps the active slot and drops the
    // previous weapon at the pickup position so equipment is never destroyed.
    const std::size_t slot = weapon_inventory_.ActiveSlot();
    if (weapon_inventory_.SlotOccupied(slot)) {
        const std::size_t old_index = weapon_inventory_.Slot(slot);
        if (old_index < weapons_.Count()) {
            (void)weapon_pickups_.Spawn(old_index, pickup_position);
            LOG_INFO_APP("Weapon pickup swap: dropped {} at ({:.2f},{:.2f})",
                         weapons_.Definitions()[old_index].id,
                         pickup_position.x, pickup_position.y);
        }
    }
    if (!weapon_inventory_.EquipActive(weapon_index)) return false;
    ConfigureActiveWeapon();
    ConfigureSecondaryWeapon();
    LOG_INFO_APP("Weapon pickup equipped into slot {}: {}",
                 slot + 1, weapons_.Definitions()[weapon_index].id);
    return true;
}

engine::Vec2 Application::AimOrigin() const {
    return player_.Position() + engine::Vec2{0.0f, 0.5f};
}

float Application::AimAngleDegrees() const {
    return std::atan2(aim_direction_.y, aim_direction_.x) *
           180.0f / 3.14159265358979323846f;
}

void Application::ConfigureSecondaryWeapon() {
    secondary_weapon_.Clear();
    if (weapons_.Count() == 0) return;
    const std::size_t active = weapon_inventory_.ActiveSlot();
    if (!weapon_inventory_.SlotOccupied(active)) return;
    const std::size_t other = active == 0 ? 1 : 0;
    // HYPOTHESIS (off-hand duplication): C01Controller owns hand2/hand3 and
    // GunInitKnightXXSkillListener maintains an off-hand sprite. The recovered
    // prefab does not expose a separate "empty off-hand" rule, so when only one
    // weapon is equipped the native dual wield presents a copy of the active
    // weapon. With two equipped weapons the other slot is used.
    const std::size_t index = weapon_inventory_.SlotOccupied(other)
        ? weapon_inventory_.Slot(other)
        : weapon_inventory_.Slot(active);
    if (index >= weapons_.Count()) return;
    engine::assets::WeaponDefinition definition = weapons_.Definitions()[index];
    if (scene_.player_stats.max_hp > 0) {
        definition.owner_deviation = scene_.player_stats.deviation;
    }
    if (!secondary_weapon_.Configure(definition)) {
        secondary_weapon_.Clear();
    } else {
        const std::size_t state_slot = weapon_inventory_.SlotOccupied(other)
            ? other : active;
        secondary_weapon_.SetPotEmpty(
            pot_empty_state_[std::min<std::size_t>(state_slot, 1)]);
    }
}

void Application::ConfigureTraderOffers() {
    trader_offers_.clear();
    if (weapons_.Count() == 0) return;
    int level = 1;
    engine::RGRandomStream random;
    random.SetRandomSeed(static_cast<int32_t>(
        (static_cast<std::uint32_t>(trader_stock_seed_) ^ 0x4F524253u) &
        0x7FFFFFFFu));
    for (int slot = 0; slot < 3; ++slot, ++level) {
        std::vector<std::pair<std::size_t, const soulknight::game::items::WeaponDropEntry*>>
            candidates;
        int total = 0;
        for (const auto& drop : item_assets_.WeaponDrops()) {
            if (drop.level != level) continue;
            for (const auto& entry : drop.entries) {
                std::size_t index = weapons_.Count();
                for (std::size_t i = 0; i < weapons_.Count(); ++i) {
                    if (weapons_.Definitions()[i].id == entry.weapon) {
                        index = i;
                        break;
                    }
                }
                if (index >= weapons_.Count()) continue;
                const auto* catalogue = weapon_catalogue_.Find(entry.weapon);
                if (catalogue == nullptr || catalogue->item_value <= 0) continue;
                candidates.emplace_back(index, &entry);
                total += entry.weight;
            }
        }
        // If the recovered drop level has no integrated weapon, use the
        // deterministic integrated set but keep the serialized item_value as
        // the price. This is a documented fallback, never a fake price.
        if (candidates.empty()) {
            for (std::size_t i = 0; i < weapons_.Count(); ++i) {
                const auto* catalogue =
                    weapon_catalogue_.Find(weapons_.Definitions()[i].id);
                if (catalogue != nullptr && catalogue->item_value > 0) {
                    candidates.emplace_back(i, nullptr);
                }
            }
        }
        if (candidates.empty()) continue;
        int roll = total > 0 ? random.Range(0, total) : 0;
        std::size_t chosen = candidates.front().first;
        for (const auto& [index, entry] : candidates) {
            if (entry == nullptr) { chosen = index; break; }
            if (roll < entry->weight) { chosen = index; break; }
            roll -= entry->weight;
        }
        const auto& definition = weapons_.Definitions()[chosen];
        const auto* catalogue = weapon_catalogue_.Find(definition.id);
        soulknight::game::ui::TraderOfferView offer;
        offer.weapon_index = static_cast<int>(chosen);
        offer.display_name = definition.display_name.empty()
            ? definition.id : definition.display_name;
        offer.sprite_index = definition.visual_sprite_index;
        offer.price = catalogue != nullptr ? catalogue->item_value : 0;
        offer.damage = definition.damage;
        offer.energy_cost = definition.EnergyCost();
        if (offer.price <= 0) continue;
        trader_offers_.push_back(std::move(offer));
    }
    std::string stock;
    for (const auto& offer : trader_offers_) {
        if (!stock.empty()) stock += ",";
        stock += std::to_string(offer.weapon_index) + ":" +
                 std::to_string(offer.price);
    }
    LOG_INFO_APP("Trader: {} orb offers generated seed={} stock={}",
                 trader_offers_.size(), trader_stock_seed_, stock);
}

void Application::BuyTraderItem() {
    const int selected = game_ui_.TraderSelection();
    if (selected < 0 ||
        static_cast<std::size_t>(selected) >= trader_offers_.size()) {
        return;
    }
    auto& offer = trader_offers_[static_cast<std::size_t>(selected)];
    if (offer.sold) {
        LOG_INFO_APP("Trader: offer already sold");
        PlayUiSound("ui_click");
        return;
    }
    if (player_orbs_ < offer.price) {
        LOG_INFO_APP("Trader: insufficient orbs ({} < {})",
                     player_orbs_, offer.price);
        PlayUiSound("ui_click");
        return;
    }
    if (offer.weapon_index < 0 ||
        static_cast<std::size_t>(offer.weapon_index) >= weapons_.Count()) {
        return;
    }
    player_orbs_ -= offer.price;
    offer.sold = true;
    PlayEffect("pickup");
    SpawnWeaponPickupAt(static_cast<std::size_t>(offer.weapon_index),
                        {player_.Position().x + 1.4f,
                         player_.Position().y - 0.1f});
    LOG_INFO_APP("Trader: bought {} for {} orbs (remaining {})",
                 weapons_.Definitions()[static_cast<std::size_t>(
                     offer.weapon_index)].display_name,
                 offer.price, player_orbs_);
    SaveGameToDisk();
}

void Application::ConfigureAbilities() {
    abilities_.Clear();
    if (player_characters_.Count() == 0 ||
        player_characters_.Active().id != "c00") {
        return;
    }
    // CONFIRMED c00 SkillInfo[0]: maxTime=5.0, cd=10.0. The C01Controller
    // hand2/hand3 paths and GunInitKnightXXSkillListener identify the Knight
    // dual-wield mechanic; skill icon is recovered ui_skill01.
    soulknight::game::player::AbilityDefinition dual;
    dual.id = "knight_dual_wield";
    dual.energy_cost = 0;
    dual.active_time = 5.0f;
    dual.cooldown = 10.0f;
    abilities_.Add(dual);
}

void Application::ActivateKnightPower() {
    if (player_dead_) return;
    if (abilities_.Count() == 0) {
        LOG_INFO_APP("Q: this character has no recovered power yet");
        return;
    }
    if (!abilities_.Activate(0, player_energy_)) {
        LOG_INFO_APP("Q: Knight power not ready");
        return;
    }
    LOG_INFO_APP("Q: KNIGHT DUAL WIELD active (5.0 s, cooldown 10.0 s)");
}

const soulknight::game::enemy::EnemyInstance* Application::FindAutoAimTarget() const {
    const engine::Vec2 origin = AimOrigin();
    const auto* best = static_cast<const soulknight::game::enemy::EnemyInstance*>(nullptr);
    float best_distance = std::numeric_limits<float>::max();
    for (const auto& enemy : enemies_.Enemies()) {
        if (!enemy.active || enemy.health.dead) continue;
        const engine::Vec2 target = enemy.position + engine::Vec2{0.0f, 0.5f};
        // Ignore targets behind recovered solid geometry.
        bool blocked = false;
        for (const auto& solid : collision_world_.Solids()) {
            float hit = 0.0f;
            if (soulknight::game::combat::ProjectileSystem::SegmentHitsAabb(
                    origin, target, solid, {}, hit)) {
                blocked = true;
                break;
            }
        }
        if (blocked) continue;
        const float distance = (target - origin).LengthSquared();
        if (distance < best_distance) {
            best_distance = distance;
            best = &enemy;
        }
    }
    return best;
}

void Application::UpdateAutoAim() {
    if (aim_mode_ != soulknight::game::player::AimMode::Auto) {
        auto_aim_has_target_ = false;
        return;
    }
    const auto* target = attack_held_ ? FindAutoAimTarget() : nullptr;
    if (target == nullptr) {
        auto_aim_has_target_ = false;
        // The reference weapon is not permanently locked to the last target;
        // with no attack (or no visible living target) it returns to the
        // horizontal held pose while the body keeps its current facing.
        aim_direction_ = facing_ == engine::locomotion::FacingDirection::Left
            ? engine::Vec2{-1.0f, 0.0f} : engine::Vec2{1.0f, 0.0f};
        return;
    }
    auto_aim_has_target_ = true;
    auto_aim_target_ = target->position + engine::Vec2{0.0f, 0.5f};
    const engine::Vec2 desired = auto_aim_target_ - AimOrigin();
    if (desired.LengthSquared() > 1e-6f) {
        aim_direction_ = desired.Normalized();
    }
}

void Application::SwitchWeaponSlot(std::size_t slot) {
    if (!weapon_inventory_.SwitchSlot(slot)) return;
    ConfigureActiveWeapon();
    ConfigureSecondaryWeapon();
    LOG_INFO_APP("Weapon slot: {} primary={} secondary={} active={}",
                 weapon_inventory_.ActiveSlot() + 1,
                 weapon_inventory_.Slot(0), weapon_inventory_.Slot(1),
                 weapons_.Count() > 0 ? weapons_.Active().id : "none");
}

void Application::ConfigureActiveWeapon() {
    if (weapons_.Count() == 0 ||
        !weapon_inventory_.SlotOccupied(weapon_inventory_.ActiveSlot())) {
        weapon_.Clear();
        LOG_INFO_APP("Weapon active: EMPTY (slot {})",
                     weapon_inventory_.ActiveSlot() + 1);
        return;
    }
    const std::size_t active_index = weapon_inventory_.ActiveIndex();
    if (!weapons_.Select(active_index)) return;
    engine::assets::WeaponDefinition weapon_definition = weapons_.Active();
    if (scene_.player_stats.max_hp > 0) {
        weapon_definition.owner_deviation = scene_.player_stats.deviation;
    }
    if (!weapon_.Configure(weapon_definition)) {
        LOG_ERROR_APP("Weapon configuration failed: {}", weapon_definition.id);
        return;
    }
    weapon_.SetPotEmpty(pot_empty_state_[
        std::min<std::size_t>(weapon_inventory_.ActiveSlot(), 1)]);
    LOG_INFO_APP("Weapon active: [{}] {} projectile={} speed={:.1f} "
                 "damage={} deviation={} consume={} cycle={:.6f}s",
                 weapons_.ActiveIndex() + 1, weapon_definition.id,
                 weapon_definition.projectile.id, weapon_definition.bullet_speed,
                 weapon_definition.damage, weapon_definition.deviation,
                 weapon_definition.consume,
                 weapon_definition.attack_cycle_seconds);
}

void Application::ResetCombat() {
    player_ = engine::locomotion::PlayerLocomotion(scene_.spawn);
    previous_player_position_ = player_.Position();
    render_player_position_ = player_.Position();
    previous_camera_position_ = scene_.spawn;
    render_camera_ = camera_;
    render_camera_.position = scene_.spawn;
    player_.SetSpeedRateBonus(0.0f);
    const int player_max_hp =
        scene_.player_stats.max_hp > 0 ? scene_.player_stats.max_hp : 6;
    const int player_max_armor =
        scene_.player_stats.max_armor > 0 ? scene_.player_stats.max_armor : 5;
    player_health_.Reset(player_max_hp, player_max_armor, 0);
    player_health_.camp =
        scene_.player_stats.max_hp > 0 ? scene_.player_stats.camp : 1;
    player_energy_.Reset(
        scene_.player_stats.max_energy > 0
            ? scene_.player_stats.max_energy : 180,
        scene_.player_stats.max_energy > 0
            ? scene_.player_stats.energy : 180);
    player_dead_ = false;
    attack_held_ = false;
    weapon_.Release();
    aim_direction_ = {1.0f, 0.0f};
    aim_mode_ = soulknight::game::player::AimMode::Auto;
    mouse_aim_valid_ = true;
    facing_ = engine::locomotion::FacingDirection::Right;
    aim_direction_ = {1.0f, 0.0f};
    previous_aim_direction_ = aim_direction_;
    render_aim_direction_ = aim_direction_;
    projectiles_.Clear();
    enemies_.Reset();
    pet_.Reset();
    training_stake_.Reset();
    dialogue_timer_ = 0.0f;
    stake_damage_timer_ = 0.0f;
    cleared_rooms_.erase(scene_.id);
    chest_spawned_rooms_.erase(scene_.id);
    room_enemy_snapshots_.erase(scene_.id);
    item_world_.Reset();
    player_coins_ = 0;
    ConfigureRoomItems();
    obstacles_.ResetOverride();
    boxes_.Reset();
    boxes_.Configure(scene_, obstacles_);
    summons_.Clear();
    mercenary_spawned_ = false;
    black_holes_.clear();
    weapon_inventory_.Configure(weapons_.Count());
    ConfigureActiveWeapon();
    ConfigureSecondaryWeapon();
    ConfigureAbilities();
    weapon_pickups_.Clear();
    player_characters_.Select(0);
    ApplyActiveCharacterStats();
    ConfigureAbilities();
    room_clear_pending_ = false;
    mech_station_open_ = false;
    room_entry_position_ = player_.Position();
    room_activation_timer_ = 0.0f;
    if (scene_.is_hub) {
        room_activation_state_ =
            soulknight::game::room::RoomActivationState::Inactive;
    } else if (!scene_.doors.empty() && enemies_.AliveCount() > 0) {
        room_activation_state_ =
            soulknight::game::room::RoomActivationState::Entered;
    } else {
        room_activation_state_ =
            soulknight::game::room::RoomActivationState::Cleared;
    }
    BeginRoomTitle(3.0f);
    if (!scene_.is_hub && !scene_.doors.empty() &&
        enemies_.AliveCount() > 0) {
        room_doors_.CloseAll();
        room_clear_pending_ = true;
    } else {
        room_doors_.OpenAll();
    }
    door_trigger_inside_.assign(room_doors_.Count(), false);
    RebuildCollisionWorld();
    camera_.position = scene_.spawn;
    ConfigureCameraBounds();
    if (camera_bounds_valid_) {
        camera_.ClampToRect(camera_bounds_.min, camera_bounds_.max);
    }
    locomotion_animator_.Configure(player_characters_.IdleClip(),
                                   player_characters_.RunClip(),
                                   player_characters_.DeathClip(),
                                   player_characters_.HitClip());
    last_animation_sprite_.clear();
    last_animation_state_ = engine::animation::LocomotionState::Idle;
    animation_logs_remaining_ = 8;
    LOG_INFO_APP("Developer reset: spawn=({:.2f},{:.2f}) hp={} armor={} "
                 "enemies={} doors=open character=c00 weapon={} "
                 "obstacle=wall603",
                 scene_.spawn.x, scene_.spawn.y, player_health_.hp,
                 player_health_.armor, enemies_.Enemies().size(),
                 weapon_.IsConfigured() ? weapon_.Definition().id : "EMPTY");
}

void Application::UpdateDoorTriggers() {
    if (multi_room_world_ && scene_.is_hub) {
        for (const engine::assets::InteractionMarker& marker :
             scene_.interaction_markers) {
            if (marker.kind != "door") continue;
            if ((marker.position - player_.Position()).LengthSquared() <=
                2.25f) {
                (void)SwitchRoom("hub_door");
                return;
            }
        }
    }
    if (!scene_.has_room_topology || scene_.doors.empty()) return;
    // The reference door trigger reacts to c00's root trigger box, offset
    // (0, 0.7). This native probe is for debug/traversal logging only.
    const engine::Vec2 probe{
        player_.Position().x,
        player_.Position().y + 0.7f
    };
    for (std::size_t i = 0; i < scene_.doors.size() &&
                            i < door_trigger_inside_.size(); ++i) {
        const engine::assets::SceneDoor& door = scene_.doors[i];
        const bool inside =
            probe.x >= door.trigger.min.x && probe.x <= door.trigger.max.x &&
            probe.y >= door.trigger.min.y && probe.y <= door.trigger.max.y;
        const bool was_inside = door_trigger_inside_[i];
        if (inside != was_inside) {
            door_trigger_inside_[i] = inside;
            if (inside && multi_room_world_ && !room_doors_.At(i).IsBlocking() &&
                room_manager_.ConnectionFromDoor(door.id) != nullptr) {
                (void)SwitchRoom(door.id);
                return;
            }
            if (config_.locomotion_debug) {
                const soulknight::game::room::Door& state =
                    room_doors_.At(i);
                LOG_INFO_APP("Door trigger {}: {} aisle={} state={} "
                             "blocking={} probe=({:.3f},{:.3f})",
                             door.id,
                             inside ? "enter" : "exit",
                             door.aisle,
                             soulknight::game::room::DoorStateName(
                                 state.State()),
                             BooleanText(state.IsBlocking()),
                             probe.x, probe.y);
            }
        }
    }
}

void Application::UpdateTrainingStake(float delta_seconds, int melee_damage) {
    training_stake_.Update(delta_seconds);
    if (!training_stake_.Configured()) return;
    const engine::Vec2 stake_position = training_stake_.Position();
    const engine::collision::Aabb stake_box{
        {stake_position.x - 0.45f, stake_position.y},
        {stake_position.x + 0.45f, stake_position.y + 2.05f}};
    const auto accept_hit = [&](int damage) {
        bool raised = false;
        if (!training_stake_.Hit(damage, player_.Position().x, &raised)) {
            return;
        }
        stake_damage_ = std::max(1, damage);
        stake_damage_timer_ = 0.8f;
        stake_damage_screen_ = render_camera_.WorldToScreen(
            stake_position + engine::Vec2{0.0f, 2.1f});
        PlayEffect("player_hit", 0.35f);
        if (raised) {
            LOG_INFO_APP("Training stake: StakeHurtEvent raised (30% chance)");
        }
    };
    bool hit_this_tick = false;
    for (auto& projectile : projectiles_.Projectiles()) {
        if (!projectile.active || projectile.camp != player_health_.camp) {
            continue;
        }
        const engine::collision::Aabb box{
            {projectile.position.x - projectile.collider_half_extents.x,
             projectile.position.y - projectile.collider_half_extents.y},
            {projectile.position.x + projectile.collider_half_extents.x,
             projectile.position.y + projectile.collider_half_extents.y}};
        if (!box.Intersects(stake_box)) continue;
        accept_hit(projectile.damage);
        projectile.active = false;
        hit_this_tick = true;
    }
    if (!hit_this_tick && melee_damage > 0) {
        const engine::Vec2 offset = stake_position +
            engine::Vec2{0.0f, 1.0f} - player_.Position();
        const float distance = offset.Length();
        if (distance <= 2.6f && distance > 0.001f) {
            const engine::Vec2 direction = offset * (1.0f / distance);
            if (direction.x * aim_direction_.x +
                direction.y * aim_direction_.y > 0.1f) {
                accept_hit(melee_damage);
            }
        }
    }
}

void Application::PopulateBiomeEnemies() {
    if (!biome_.IsLoaded() || scene_.is_hub) return;
    // Start/end/chest/special rooms contain their own recovered objects. Boss
    // rooms keep their recovered BossCreator fixture (the boss) instead of the
    // normal-room biome pool.
    const bool boss_room = scene_.id.rfind("r1_boss", 0) == 0 ||
                           scene_.id.rfind("r2_boss", 0) == 0 ||
                           scene_.id.rfind("r3_boss", 0) == 0;
    if (boss_room) return;
    if (scene_.room.type != 1) {
        enemies_.ClearEnemies();
        return;
    }
    const soulknight::game::room::BiomeLevel* level = biome_.Current();
    if (level == nullptr || level->enemies.empty()) return;
    enemies_.ClearEnemies();
    std::uint32_t hash = 2166136261u;
    for (char c : scene_.id) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 16777619u;
    }
    hash ^= static_cast<std::uint32_t>(map_seed_) * 0x9E3779B9u;
    hash ^= static_cast<std::uint32_t>(biome_.CurrentIndex()) * 0x85EBCA6Bu;
    engine::RGRandomStream random;
    random.SetRandomSeed(static_cast<std::int32_t>(hash & 0x7FFFFFFFu));
    const engine::Vec2 spawn = scene_.spawn;
    const engine::Vec2 offsets[] = {
        {-4.0f, 0.0f}, {4.0f, 0.0f}, {0.0f, 4.0f}, {0.0f, -4.0f},
        {-5.0f, 4.0f}, {5.0f, 4.0f}, {-5.0f, -4.0f}, {5.0f, -4.0f},
    };
    const engine::collision::Aabb bounds{
        {scene_.interior_bounds.x, scene_.interior_bounds.y},
        {scene_.interior_bounds.x + scene_.interior_bounds.w,
         scene_.interior_bounds.y + scene_.interior_bounds.h}};
    const auto free_position = [&](const engine::Vec2& position) {
        const engine::collision::Aabb body{
            {position.x - 0.45f, position.y - 0.1f},
            {position.x + 0.45f, position.y + 0.9f}};
        if (bounds.IsValid() &&
            (body.min.x < bounds.min.x || body.max.x > bounds.max.x ||
             body.min.y < bounds.min.y || body.max.y > bounds.max.y)) {
            return false;
        }
        for (const auto& solid : collision_world_.Solids()) {
            if (body.Intersects(solid)) return false;
        }
        return true;
    };
    int spawned = 0;
    for (int attempt = 0; attempt < 12 && spawned < 3; ++attempt) {
        const std::string enemy_id =
            biome_.PickEnemy(random.Range(0, 10000));
        std::size_t definition = enemies_.DefinitionCount();
        for (std::size_t i = 0; i < enemies_.DefinitionCount(); ++i) {
            if (enemies_.DefinitionAt(i).id == enemy_id) {
                definition = i;
                break;
            }
        }
        if (definition >= enemies_.DefinitionCount()) continue;
        const engine::Vec2 candidate = spawn +
            offsets[static_cast<std::size_t>(attempt) %
                    (sizeof(offsets) / sizeof(offsets[0]))];
        if (!free_position(candidate)) continue;
        if (enemies_.SpawnEnemy(definition, candidate)) ++spawned;
    }
    LOG_INFO_APP("Biome {}: spawned {} enemies from {} weighted pool entries",
                 level->id, spawned, level->enemies.size());
}

void Application::RunGameplaySmokeStep() {
    if (gameplay_smoke_stage_time_ <= 0.0) {
        gameplay_smoke_stage_time_ = simulation_time_ + 0.8;
        return;
    }
    if (simulation_time_ < gameplay_smoke_stage_time_) return;
    gameplay_smoke_stage_time_ = simulation_time_ + 0.8;
    switch (gameplay_smoke_stage_) {
        case 0: {
            const bool hub = scene_.is_hub;
            const bool pets = pet_.Count() > 0;
            const bool recipes = workshop_.Recipes().size() >= 200;
            LOG_INFO_APP("Gameplay smoke hub: room={} pets={} recipes={}",
                         scene_.id, pets ? pet_.Count() : 0,
                         workshop_.Recipes().size());
            if (!hub || !pets || !recipes) {
                LOG_ERROR_APP("Gameplay smoke hub checks failed");
                exit_code_ = 1;
                running_ = false;
                return;
            }
            if (pet_.Count() > 0) {
                pet_.SelectSaved(active_pet_id_);
                active_pet_id_ = pet_.SelectedId();
            }
            if (weapons_.Count() > 0) {
                (void)weapon_inventory_.EquipActive(0);
                ConfigureActiveWeapon();
                if (workshop_.AddMaterialsForDismantle(
                        weapons_.Definitions()[0].id) == nullptr) {
                    LOG_WARN_APP("Gameplay smoke: no dismantle recipe for {}",
                                 weapons_.Definitions()[0].id);
                }
            }
            LOG_INFO_APP("Gameplay smoke: pet={} materials={}",
                         active_pet_id_, workshop_.SnapshotMaterials().size());
            break;
        }
        case 1: {
            if (plant_system_.Plots().empty()) {
                LOG_ERROR_APP("Gameplay smoke: garden plots missing");
                exit_code_ = 1;
                running_ = false;
                return;
            }
            std::string seed = plant_system_.FirstAvailableSeed();
            if (seed.empty()) {
                seed = "plant_carrot";
                plant_system_.AddSeed(seed, 1);
            }
            const auto* plant = item_assets_.FindPlant(seed);
            const int plot_index = plant_system_.NearestPlot(
                plant_system_.Plots().front().position, 0.1f);
            const auto planted = plant_system_.Interact(
                plot_index, seed,
                plant != nullptr ? plant->growth_seconds : 30.0f,
                plant != nullptr
                    ? static_cast<int>(plant->states.size()) : 2,
                plant != nullptr ? plant->item_id : std::string{},
                plant != nullptr ? plant->harvest_count : 1);
            plant_system_.Update(60.0f);
            const auto harvested = plant_system_.Interact(plot_index, {});
            LOG_INFO_APP("Gameplay smoke garden: seed={} planted={} "
                         "harvested={} count={}",
                         seed,
                         planted == game::hub::PlantInteraction::Planted
                             ? "yes" : "no",
                         harvested == game::hub::PlantInteraction::Harvested
                             ? "yes" : "no",
                         plant_system_.HarvestedCount());
            if (planted != game::hub::PlantInteraction::Planted ||
                harvested != game::hub::PlantInteraction::Harvested) {
                exit_code_ = 1;
                running_ = false;
                return;
            }
            break;
        }
        case 2: {
            const engine::assets::InteractionMarker* npc = nullptr;
            const std::string active_character =
                player_characters_.Count() > 0
                    ? player_characters_.Active().id : std::string{};
            for (const auto& marker : scene_.interaction_markers) {
                if (marker.kind == "npc" &&
                    marker.character_id != active_character) {
                    npc = &marker;
                    break;
                }
            }
            if (npc == nullptr) {
                LOG_ERROR_APP("Gameplay smoke: no Living Room NPC");
                exit_code_ = 1;
                running_ = false;
                return;
            }
            TeleportForReview(npc->position + engine::Vec2{0.0f, -0.6f});
            TryInteract();
            const bool talked = dialogue_timer_ > 0.0f &&
                !dialogue_text_.empty();
            LOG_INFO_APP("Gameplay smoke NPC: {} line={} talked={}",
                         npc->speaker, dialogue_text_, BooleanText(talked));
            if (!talked) {
                exit_code_ = 1;
                running_ = false;
                return;
            }
            dialogue_timer_ = 0.0f;
            break;
        }
        case 3: {
            if (trader_offers_.empty()) {
                LOG_ERROR_APP("Gameplay smoke: trader has no offers");
                exit_code_ = 1;
                running_ = false;
                return;
            }
            player_orbs_ = std::max(player_orbs_, 999);
            game_ui_.OpenTrader();
            BuyTraderItem();
            const bool sold = trader_offers_.front().sold;
            game_ui_.OpenGameplay();
            LOG_INFO_APP("Gameplay smoke trader: offers={} first_sold={} "
                         "orbs={}", trader_offers_.size(),
                         BooleanText(sold), player_orbs_);
            if (!sold) {
                exit_code_ = 1;
                running_ = false;
                return;
            }
            break;
        }
        case 4: {
            player_orbs_ = std::max(player_orbs_, 999);
            active_mount_id_.clear();
            OpenMechStation();
            CycleMechSelection(0);
            BuildMech();
            mech_station_open_ = false;
            const bool built = !active_mount_id_.empty();
            LOG_INFO_APP("Gameplay smoke mech: active={} orbs={} status={}",
                         active_mount_id_, player_orbs_, mech_status_);
            if (!built) {
                exit_code_ = 1;
                running_ = false;
                return;
            }
            active_mount_id_.clear();
            mount_speed_bonus_ = 0.0f;
            break;
        }
        case 5:
            if (!SwitchRoom("hub_door")) {
                LOG_ERROR_APP("Gameplay smoke: hub door transition failed");
                exit_code_ = 1;
                running_ = false;
                return;
            }
            break;
        case 6: {
            int floor_surfaces = 0;
            for (const auto& surface : level_surfaces_) {
                if (surface.sorting_layer == 2) ++floor_surfaces;
            }
            const std::string level = biome_.Current() != nullptr
                ? biome_.Current()->id : std::string{};
            const auto stage_label = room_stage_labels_.find(scene_.id);
            const bool start_message = room_title_timer_ > 0.0f &&
                stage_label != room_stage_labels_.end() &&
                stage_label->second == "1-1";
            LOG_INFO_APP("Gameplay smoke dungeon: room={} type={} enemies={} "
                         "floor_surfaces={} level={} activation={} "
                         "start_message={} label={}",
                         scene_.id, scene_.room.type, enemies_.AliveCount(),
                         floor_surfaces, level,
                         soulknight::game::room::RoomActivationName(
                             room_activation_state_),
                         BooleanText(start_message),
                         stage_label != room_stage_labels_.end()
                             ? stage_label->second : "none");
            if (floor_surfaces == 0 || scene_.is_hub || level != "1-1" ||
                !start_message) {
                LOG_ERROR_APP("Gameplay smoke: dungeon 1-1 initialization "
                              "failed");
                exit_code_ = 1;
                running_ = false;
                return;
            }
            if (enemies_.AliveCount() > 0) {
                room_activation_state_ = soulknight::game::room::
                    RoomActivationState::Combat;
                room_doors_.CloseAll();
                room_clear_pending_ = true;
                RebuildCollisionWorld();
                enemies_.ClearEnemies();
                room_activation_state_ = soulknight::game::room::
                    RoomActivationState::Cleared;
                UpdateRoomClear();
                room_clear_pending_ = false;
                cleared_rooms_.insert(scene_.id);
            } else {
                UpdateRoomClear();
                room_clear_pending_ = false;
                cleared_rooms_.insert(scene_.id);
            }
            break;
        }
        case 7: {
            const Room* combat_room = nullptr;
            for (const Room& room : room_manager_.Rooms()) {
                if (room.type != 1 || room.id == scene_.id ||
                    room.id == world_hub_id_) {
                    continue;
                }
                combat_room = &room;
                break;
            }
            if (combat_room == nullptr) {
                LOG_ERROR_APP("Gameplay smoke: no combat room in 1-1 graph");
                exit_code_ = 1;
                running_ = false;
                return;
            }
            if (!ActivateRoom(combat_room->id, "")) {
                LOG_ERROR_APP("Gameplay smoke: combat room activation failed");
                exit_code_ = 1;
                running_ = false;
                return;
            }
            LOG_INFO_APP("Gameplay smoke combat entry: room={} state={} "
                         "doors_open={} enemies={}",
                         scene_.id,
                         soulknight::game::room::RoomActivationName(
                             room_activation_state_),
                         BooleanText(room_doors_.TargetOpen()),
                         enemies_.AliveCount());
            if (enemies_.AliveCount() == 0 ||
                room_doors_.TargetOpen() ||
                room_activation_state_ !=
                    soulknight::game::room::RoomActivationState::Entered) {
                LOG_ERROR_APP("Gameplay smoke: combat room did not start "
                              "closed with dormant enemies");
                exit_code_ = 1;
                running_ = false;
                return;
            }
            break;
        }
        case 8: {
            const bool combat = room_activation_state_ ==
                soulknight::game::room::RoomActivationState::Combat;
            const int enemies_before =
                static_cast<int>(enemies_.AliveCount());
            LOG_INFO_APP("Gameplay smoke combat active: state={} enemies={} "
                         "doors_open={}", soulknight::game::room::
                         RoomActivationName(room_activation_state_),
                         enemies_before, BooleanText(room_doors_.TargetOpen()));
            if (!combat || enemies_before == 0 ||
                room_doors_.TargetOpen()) {
                exit_code_ = 1;
                running_ = false;
                return;
            }
            enemies_.ClearEnemies();
            UpdateRoomClear();
            const bool opened = room_doors_.TargetOpen();
            LOG_INFO_APP("Gameplay smoke combat clear: doors_open={} "
                         "state={} chests={}", BooleanText(opened),
                         soulknight::game::room::RoomActivationName(
                             room_activation_state_),
                         item_world_.Chests().size());
            if (!opened || room_activation_state_ !=
                    soulknight::game::room::RoomActivationState::Cleared) {
                exit_code_ = 1;
                running_ = false;
                return;
            }
            break;
        }
        case 9: {
            if (item_assets_.IsLoaded()) {
                item_world_.AddChest("chest01",
                                     scene_.spawn + engine::Vec2{0.0f, 2.0f});
                (void)item_world_.OpenNearestChest(
                    scene_.spawn + engine::Vec2{0.0f, 2.0f}, 2.5f);
                item_world_.Update(1.1f, scene_.spawn, item_assets_);
                const auto drops = item_world_.TakePendingWeaponDrops();
                LOG_INFO_APP("Gameplay smoke chest: opened, drops={}",
                             drops.size());
            }
            break;
        }
        case 10: {
            const Room* end_room = nullptr;
            for (const Room& room : room_manager_.Rooms()) {
                if (room.type == 2) {
                    end_room = &room;
                    break;
                }
            }
            if (end_room == nullptr) {
                LOG_ERROR_APP("Gameplay smoke: no end room in graph");
                exit_code_ = 1;
                running_ = false;
                return;
            }
            gameplay_smoke_level_before_ = biome_.Current() != nullptr
                ? biome_.Current()->id : std::string{};
            if (!ActivateRoom(end_room->id, "")) {
                LOG_ERROR_APP("Gameplay smoke: end room activation failed");
                exit_code_ = 1;
                running_ = false;
                return;
            }
            LOG_INFO_APP("Gameplay smoke end room: {}", scene_.id);
            break;
        }
        case 11: {
            const bool advanced = AdvanceDungeonLevel();
            const std::string level_after = biome_.Current() != nullptr
                ? biome_.Current()->id : std::string{};
            LOG_INFO_APP("Gameplay smoke progression: {} -> {} ({})",
                         gameplay_smoke_level_before_, level_after,
                         advanced ? "advanced" : "final");
            if (!advanced || level_after != "1-2") {
                exit_code_ = 1;
                running_ = false;
                return;
            }
            break;
        }
        case 12: {
            SaveGameToDisk();
            const std::string scene_before = scene_.id;
            const std::string level_before = biome_.Current() != nullptr
                ? biome_.Current()->id : std::string{};
            // Simulate a fresh process: reset the in-memory campaign before
            // loading so the save must restore the level itself.
            biome_.Reset();
            LoadGameFromDisk();
            const bool ok = scene_.id == scene_before &&
                !save_slot_summary_.empty() &&
                biome_.Current() != nullptr &&
                biome_.Current()->id == level_before;
            LOG_INFO_APP("Gameplay smoke save/load: scene={} ok={} level={}",
                         scene_.id, ok ? "yes" : "no",
                         biome_.Current() != nullptr
                             ? biome_.Current()->id : "none");
            if (!ok) exit_code_ = 1;
            break;
        }
        case 13: {
            const bool returned = ActivateRoom(world_hub_id_, "");
            LOG_INFO_APP("Gameplay smoke return hub: ok={} scene={} "
                         "garden_plots={}",
                         BooleanText(returned), scene_.id,
                         plant_system_.Plots().size());
            if (!returned || !scene_.is_hub ||
                plant_system_.Plots().empty()) {
                exit_code_ = 1;
            }
            break;
        }
        default:
            LOG_INFO_APP("Gameplay smoke: COMPLETE");
            running_ = false;
            return;
    }
    ++gameplay_smoke_stage_;
}

void Application::FixedUpdate(double dt) {
    simulation_time_ += dt;
    ++simulation_ticks_;
    if (config_.gameplay_smoke) RunGameplaySmokeStep();

    if (!scene_mode_) return;
    hit_flash_timer_ = std::max(0.0f, hit_flash_timer_ - static_cast<float>(dt));
    stealth_timer_ = std::max(0.0f, stealth_timer_ - static_cast<float>(dt));
    attack_speed_timer_ =
        std::max(0.0f, attack_speed_timer_ - static_cast<float>(dt));
    camera_flash_timer_ =
        std::max(0.0f, camera_flash_timer_ - static_cast<float>(dt));
    attack_speed_multiplier_ = attack_speed_timer_ > 0.0f
        ? std::max(attack_speed_multiplier_, 1.0f) : 1.0f;
    weapon_.SetAttackSpeedMultiplier(attack_speed_multiplier_);
    secondary_weapon_.SetAttackSpeedMultiplier(attack_speed_multiplier_);
    room_title_timer_ = std::max(0.0f, room_title_timer_ - static_cast<float>(dt));
    room_clear_timer_ = std::max(0.0f, room_clear_timer_ - static_cast<float>(dt));
    room_transition_flash_ = std::max(0.0f,
        room_transition_flash_ - static_cast<float>(dt));
    player_energy_.Update(static_cast<float>(dt));
    asset_lab_.Update(static_cast<float>(dt));
    weapon_pickups_.Update(static_cast<float>(dt));
    plant_system_.Update(static_cast<float>(dt));
    dialogue_timer_ = std::max(0.0f, dialogue_timer_ - static_cast<float>(dt));
    stake_damage_timer_ =
        std::max(0.0f, stake_damage_timer_ - static_cast<float>(dt));

    room_activation_timer_ += static_cast<float>(dt);
    const auto previous_activation = room_activation_state_;
    room_activation_state_ = soulknight::game::room::AdvanceRoomActivation(
        room_activation_state_, room_activation_timer_,
        room_entry_position_, player_.Position(),
        enemies_.AliveCount() > 0, enemies_.AliveCount() == 0);
    if (previous_activation ==
            soulknight::game::room::RoomActivationState::Entered &&
        room_activation_state_ ==
            soulknight::game::room::RoomActivationState::Combat) {
        room_doors_.CloseAll();
        room_clear_pending_ = true;
        RebuildCollisionWorld();
        PlayEffect("ui_click", 0.35f);
        LOG_INFO_APP("Room activation: {} combat begins, doors closed",
                     scene_.id);
    }
    if (room_doors_.Update(static_cast<float>(dt))) {
        RebuildCollisionWorld();
        if (config_.locomotion_debug) {
            for (std::size_t i = 0; i < room_doors_.Count(); ++i) {
                const soulknight::game::room::Door& door = room_doors_.At(i);
                LOG_INFO_APP("Door {}: state={} blocking={} t={:.3f}",
                             door.Data().id,
                             soulknight::game::room::DoorStateName(
                                 door.State()),
                             BooleanText(door.IsBlocking()),
                             door.AnimationTime());
            }
        }
    }

    previous_player_position_ = player_.Position();
    previous_camera_position_ = camera_.position;
    abilities_.Update(static_cast<float>(dt));
    previous_aim_direction_ = aim_direction_;
    UpdateAutoAim();
    if (!player_dead_) {
        player_.SetSpeedRateBonus(
            (attack_held_ ? weapon_.Definition().atk_move_speed : 0.0f) +
            mount_speed_bonus_);
        player_.FixedUpdate(movement_input_.direction, movement_input_.weight,
                            static_cast<float>(dt), collision_world_);
        // Body orientation follows movement while moving; aim only chooses the
        // idle facing. Weapon aim remains independent. The recovered c00
        // controller contains only side-view idle/run/dead/hit clips, so the
        // body uses horizontal mirroring rather than invented up/down clips.
        facing_ = engine::locomotion::ResolveBodyFacing(
            facing_, movement_input_.direction, aim_direction_,
            player_.IsMoving());
        locomotion_animator_.Update(player_.IsMoving(), static_cast<float>(dt));
        const int shots = weapon_.Update(static_cast<float>(dt), attack_held_,
                                         player_.Position(), aim_direction_,
                                         projectiles_, &player_energy_);
        for (const soulknight::game::combat::WeaponFireEvent& event :
             weapon_.TakeFireEvents()) {
            const engine::assets::WeaponSpecial& special =
                weapon_.Definition().special;
            if (event.kind == "pot_drink") {
                const auto restored = soulknight::game::items::ApplyRestore(
                    special.hp, special.energy, player_health_.hp,
                    player_health_.max_hp, player_energy_.energy,
                    player_energy_.max_energy);
                if (!special.audio_clip.empty()) {
                    PlayEffect("weapon_" + special.audio_clip, 0.55f);
                }
                ShowDialogue("POTION", "+" +
                             std::to_string(restored.health_gained) + " HP  +" +
                             std::to_string(restored.energy_gained) + " ENERGY",
                             1.8f);
                pot_empty_state_[std::min<std::size_t>(
                    weapon_inventory_.ActiveSlot(), 1)] = true;
                LOG_INFO_APP("GunPot: healed {} hp, {} energy; empty",
                             restored.health_gained, restored.energy_gained);
            } else if (event.kind == "charge_start") {
                if (!special.audio_clip.empty()) {
                    PlayEffect("weapon_" + special.audio_clip, 0.45f);
                }
            } else if (event.kind == "charge_release") {
                const std::string& clip = special.audio_clip2.empty()
                    ? weapon_.Definition().audio_clip : special.audio_clip2;
                if (!clip.empty()) PlayEffect("weapon_" + clip, 0.55f);
            } else if (event.kind == "charge_full") {
                const std::string& clip = special.audio_clip2.empty()
                    ? weapon_.Definition().audio_clip : special.audio_clip2;
                if (!clip.empty()) PlayEffect("weapon_" + clip, 0.62f);
            } else if (event.kind == "end_shot") {
                if (!special.audio_clip2.empty()) {
                    PlayEffect("weapon_" + special.audio_clip2, 0.55f);
                }
            } else if (event.kind == "summon_skeleton" ||
                event.kind == "summon_phantom") {
                const int spawned = summons_.Spawn(
                    special, player_.Position(),
                    static_cast<std::size_t>(std::max(1, special.count)),
                    false);
                LOG_INFO_APP("Weapon summon {}: spawned {} actor(s) life={:.1f}s",
                             event.kind, spawned, special.duration);
            } else if (event.kind == "funnel_swords") {
                const int spawned = summons_.Spawn(
                    special, player_.Position(),
                    static_cast<std::size_t>(std::max(1, special.count)),
                    true);
                LOG_INFO_APP("Funnel swords: spawned {} for {:.1f}s",
                             spawned, special.duration);
            } else if (event.kind == "stealth") {
                stealth_timer_ = std::max(stealth_timer_, special.duration);
                LOG_INFO_APP("BuffStealth: {:.1f}s alpha=0.297",
                             special.duration);
            } else if (event.kind == "attack_speed") {
                attack_speed_timer_ =
                    std::max(attack_speed_timer_, special.duration);
                attack_speed_multiplier_ = 1.0f +
                    std::max(0.0f, special.value) / 100.0f;
                PlayEffect("weapon_fire", 0.45f);
                LOG_INFO_APP("BuffAtkSpeedChange: +{:.0f}% for {:.1f}s",
                             special.value, special.duration);
            } else if (event.kind == "camera_flash") {
                const int hits = enemies_.ApplyAreaDamage(
                    player_.Position(), special.range, special.damage, 1);
                camera_flash_timer_ = std::max(camera_flash_timer_,
                                               special.duration);
                PlayEffect("weapon_fire", 0.5f);
                LOG_INFO_APP("BulletFullScreen camera: {} hit(s) range={:.1f}",
                             hits, special.range);
            } else if (event.kind == "black_hole") {
                const engine::Vec2 aim = aim_direction_.LengthSquared() > 1e-8f
                    ? aim_direction_.Normalized() : engine::Vec2{1.0f, 0.0f};
                BlackHoleState hole;
                hole.position = player_.Position() + aim * 2.2f;
                hole.duration = std::max(0.05f, special.duration);
                hole.radius = 3.5f;
                hole.damage = special.damage > 0 ? special.damage : 16;
                black_holes_.push_back(hole);
                PlayEffect("weapon_fire", 0.5f);
                LOG_INFO_APP("GunChannel: black hole at ({:.2f},{:.2f}) end={:.2f}s",
                             hole.position.x, hole.position.y, hole.duration);
            } else if (event.kind == "box_place") {
                const engine::Vec2 aim = aim_direction_.LengthSquared() > 1e-8f
                    ? aim_direction_.Normalized() : engine::Vec2{1.0f, 0.0f};
                const engine::Vec2 position = player_.Position() +
                    aim * std::max(0.0f, special.value);
                if (boxes_.DeployBox(position,
                                     special.hp > 0 ? special.hp : 4,
                                     special.sprite_index)) {
                    RebuildCollisionWorld();
                    PlayEffect("weapon_fire", 0.4f);
                    LOG_INFO_APP("GunBox: deployed box hp={} at ({:.2f},{:.2f})",
                                 special.hp, position.x, position.y);
                }
            } else if (event.kind == "death_mark") {
                const engine::Vec2 aim = aim_direction_.LengthSquared() > 1e-8f
                    ? aim_direction_.Normalized() : engine::Vec2{1.0f, 0.0f};
                const std::size_t target = enemies_.NearestEnemy(
                    player_.Position() + aim * 2.0f, 13.0f);
                if (target < enemies_.Enemies().size()) {
                    (void)enemies_.MarkForDeath(target, special.duration);
                    LOG_INFO_APP("BuffDeadNote: enemy {} marked for {:.1f}s",
                                 target, special.duration);
                }
            }
        }
        const bool dual_active = abilities_.Count() > 0 &&
            abilities_.Abilities()[0].state ==
                soulknight::game::player::AbilityState::Active;
        if (dual_active && secondary_weapon_.IsConfigured()) {
            const engine::Vec2 offhand{
                -aim_direction_.y * 0.34f, aim_direction_.x * 0.34f};
            const int secondary_shots = secondary_weapon_.Update(
                static_cast<float>(dt), attack_held_,
                player_.Position() + offhand,
                aim_direction_, projectiles_, &player_energy_);
            if (secondary_shots > 0) {
                const std::string& clip =
                    secondary_weapon_.Definition().audio_clip;
                PlayEffect(clip.empty() ? "weapon_fire" : "weapon_" + clip,
                           0.52f);
                LOG_INFO_APP("Dual wield: secondary fired {} shot(s)",
                             secondary_shots);
                if (secondary_weapon_.Definition().is_melee) {
                    (void)enemies_.ApplyMelee(
                        player_.Position(), aim_direction_,
                        secondary_weapon_.Definition().melee_range,
                        secondary_weapon_.Definition().melee_arc_degrees,
                        secondary_weapon_.Definition().damage, 1);
                }
            }
        } else if (!dual_active) {
            secondary_weapon_.Release();
        }
        if (shots > 0 &&
            weapon_.Definition().special.kind.rfind("charge_", 0) != 0) {
            const std::string& clip = weapon_.Definition().audio_clip;
            PlayEffect(clip.empty() ? "weapon_fire" : "weapon_" + clip, 0.62f);
        }
        const std::string& special_kind =
            weapon_.Definition().special.kind;
        const bool melee_attack = weapon_.Definition().is_melee &&
            (special_kind.empty() || special_kind == "bayonet");
        if (shots > 0 && melee_attack) {
            const int hits = enemies_.ApplyMelee(
                player_.Position(), aim_direction_,
                weapon_.Definition().melee_range,
                weapon_.Definition().melee_arc_degrees,
                weapon_.Definition().damage, 1);
            if (hits > 0) {
                LOG_INFO_APP("Melee: {} hit(s) range={:.2f}",
                             hits, weapon_.Definition().melee_range);
            }
        }
        if (shots > 0 && config_.locomotion_debug) {
            LOG_INFO_APP("Attack: fired={} projectile={} speed={:.1f} "
                         "damage={} cycle={:.6f}s first={:.6f}s "
                         "aim=({:.3f},{:.3f})",
                         shots, weapon_.Definition().projectile.id,
                         weapon_.Definition().bullet_speed,
                         weapon_.Definition().damage,
                         weapon_.Definition().attack_cycle_seconds,
                         weapon_.Definition().first_shot_delay_seconds,
                         aim_direction_.x, aim_direction_.y);
        }
    } else {
        attack_held_ = false;
        weapon_.Release();
        player_.SetSpeedRateBonus(0.0f);
        player_.FixedUpdate({0.0f, 0.0f}, 0.0f,
                            static_cast<float>(dt), collision_world_);
        locomotion_animator_.Update(false, static_cast<float>(dt));
    }

    camera_.Follow(player_.Position() + engine::Vec2{0.0f, 0.55f},
                   static_cast<float>(dt));
    if (camera_bounds_valid_) {
        camera_.ClampToRect(camera_bounds_.min, camera_bounds_.max);
    }

    projectiles_.FixedUpdate(static_cast<float>(dt), collision_world_);
    if (config_.locomotion_debug &&
        (projectiles_.WorldHitsThisStep() > 0 ||
         projectiles_.LifetimeExpirationsThisStep() > 0)) {
        LOG_INFO_APP("Projectiles: world_hits={} lifetime_expired={} active={}",
                     projectiles_.WorldHitsThisStep(),
                     projectiles_.LifetimeExpirationsThisStep(),
                     projectiles_.ActiveCount());
    }
    const bool combat_active =
        room_activation_state_ ==
            soulknight::game::room::RoomActivationState::Combat ||
        room_activation_state_ ==
            soulknight::game::room::RoomActivationState::Cleared ||
        room_activation_state_ ==
            soulknight::game::room::RoomActivationState::Inactive;
    bool player_damaged = false;
    if (combat_active) {
        enemies_.ResolveProjectileHits(projectiles_);
        player_damaged = enemies_.FixedUpdate(
            static_cast<float>(dt), player_.Position(), player_health_,
            &collision_world_, &projectiles_, stealth_timer_ <= 0.0f);
        player_damaged = projectiles_.ResolvePlayerHits(
            player_.Position(), player_health_) || player_damaged;
    }
    if (pet_.Count() > 0 && combat_active) {
        const std::size_t pet_target = pet_.Update(
            static_cast<float>(dt), player_.Position(),
            facing_ == engine::locomotion::FacingDirection::Left, enemies_,
            &collision_world_);
        if (pet_target < enemies_.Enemies().size()) {
            LOG_INFO_APP("Pet {} hit enemy {} for {}", pet_.SelectedId(),
                         pet_target, pet_.Selected()->damage);
            PlayEffect("weapon_fire", 0.35f);
        }
    }
    if (!summons_.Summons().empty()) {
        (void)summons_.Update(static_cast<float>(dt), player_.Position(),
                              enemies_, &collision_world_);
    }
    for (BlackHoleState& hole : black_holes_) {
        if (!hole.active) continue;
        hole.age += static_cast<float>(dt);
        (void)enemies_.ApplyPull(hole.position, hole.radius, 7.0f,
                                 static_cast<float>(dt));
        if (hole.age + 1e-4f >= hole.duration) {
            const int hits = enemies_.ApplyAreaDamage(
                hole.position, hole.radius, hole.damage, 1);
            hole.active = false;
            PlayEffect("weapon_fire", 0.55f);
            LOG_INFO_APP("GunChannel end shot: {} hit(s) damage={}",
                         hits, hole.damage);
        }
    }
    black_holes_.erase(
        std::remove_if(black_holes_.begin(), black_holes_.end(),
                       [](const BlackHoleState& hole) {
                           return !hole.active;
                       }),
        black_holes_.end());
    if (item_assets_.IsLoaded()) {
        if (item_world_.AutoOpenNear(player_.Position(), 2.0f)) {
            LOG_INFO_APP("Chest triggered by player proximity");
        }
        item_world_.Update(static_cast<float>(dt), player_.Position(),
                           item_assets_);
        const int collected = item_world_.CollectCoins(
            player_.Position(), 0.75f, item_assets_);
        if (collected > 0) {
            player_coins_ += collected;
            PlayEffect("pickup");
            LOG_INFO_APP("Coin collected: +{} total={}", collected,
                         player_coins_);
        }
        for (const soulknight::game::items::WeaponLootDrop& drop :
             item_world_.TakePendingWeaponDrops()) {
            std::size_t weapon_index = weapons_.Count();
            for (std::size_t i = 0; i < weapons_.Count(); ++i) {
                if (weapons_.Definitions()[i].id == drop.weapon) {
                    weapon_index = i;
                    break;
                }
            }
            if (weapon_index < weapons_.Count()) {
                SpawnWeaponPickupAt(weapon_index, drop.position);
                LOG_INFO_APP("Chest loot: spawned actual weapon {} ({})",
                             drop.weapon,
                             weapons_.Definitions()[weapon_index].display_name);
            } else {
                LOG_WARN_APP("Chest loot: {} is catalogue-only in this build",
                             drop.weapon);
            }
        }
    }
    bool box_destroyed = false;
    if (!boxes_.Boxes().empty() || !boxes_.Bubbles().empty()) {
        (void)boxes_.ApplyProjectiles(projectiles_, player_health_.camp);
        if (!player_dead_ && attack_held_ && weapon_.IsConfigured() &&
            weapon_.IsAttacking()) {
            (void)boxes_.ApplyMelee(
                player_.Position(), aim_direction_,
                weapon_.Definition().is_melee
                    ? weapon_.Definition().melee_range : 1.2f,
                weapon_.Definition().is_melee
                    ? weapon_.Definition().melee_arc_degrees : 360.0f,
                weapon_.Definition().damage);
        }
        for (const auto& box : boxes_.Boxes()) {
            if (!box.alive && obstacles_.MarkerAlive(box.marker_index)) {
                obstacles_.DestroyMarker(box.marker_index);
                box_destroyed = true;
            }
        }
        const int absorbed = boxes_.Update(static_cast<float>(dt),
                                           player_.Position());
        if (absorbed > 0) {
            player_energy_.energy = std::min(
                player_energy_.max_energy,
                player_energy_.energy + absorbed);
            PlayEffect("pickup", 0.6f);
            LOG_INFO_APP("Energy bubble absorbed: +{} (energy {}/{})",
                         absorbed, player_energy_.energy,
                         player_energy_.max_energy);
        }
        if (box_destroyed) RebuildCollisionWorld();
    }
    int stake_melee_damage = 0;
    if (!player_dead_ && attack_held_ && weapon_.IsConfigured() &&
        weapon_.IsAttacking()) {
        const float distance =
            (training_stake_.Position() - player_.Position()).Length();
        if (distance <= std::max(2.0f, weapon_.Definition().melee_range + 0.6f)) {
            stake_melee_damage = weapon_.Definition().damage;
        }
    }
    UpdateTrainingStake(static_cast<float>(dt), stake_melee_damage);
    UpdateAudio();
    UpdateRoomClear();
    if (player_damaged) {
        PlayEffect("player_hit");
        hit_flash_timer_ = 0.12f;
        (void)locomotion_animator_.PlayHit();
        LOG_INFO_APP("Player damaged: hp={} armor={} dead={}",
                     player_health_.hp, player_health_.armor,
                     BooleanText(player_health_.dead));
    }
    if (player_health_.dead && !player_dead_ &&
        !revive_used_ &&
        weapon_.Definition().special.kind == "revive") {
        revive_used_ = true;
        player_health_.Reset(player_health_.max_hp,
                             player_health_.max_armor,
                             player_health_.defence);
        player_health_.camp = 1;
        PlayEffect("heal", 0.7f);
        LOG_INFO_APP("GunRebornCross: revived player with {} HP",
                     player_health_.hp);
    }
    if (player_health_.dead && !player_dead_) {
        player_dead_ = true;
        if (!locomotion_animator_.PlayDead()) {
            LOG_INFO_APP("Character has no recovered death clip; using dead sprite");
        }
        LOG_INFO_APP("Player died: hp=0 armor={} position=({:.2f},{:.2f})",
                     player_health_.armor, player_.Position().x,
                     player_.Position().y);
    }
    UpdateDoorTriggers();
    RecordInteractiveTestTick();

    if (config_.locomotion_debug && simulation_ticks_ % 25 == 0) {
        const engine::animation::SpriteKeyframe* frame =
            locomotion_animator_.Evaluate("c00/img/body");
        const auto& collision = player_.LastCollision();
        const engine::Vec2 body = engine::rendering::ResolvePlayerRenderPosition(
            scene_, player_render_, player_render_.body_instance,
            player_.Position());
        const engine::Vec2 shadow = engine::rendering::ResolvePlayerRenderPosition(
            scene_, player_render_, player_render_.shadow_instance,
            player_.Position());
        LOG_INFO_APP(
            "locomotion: sim=({:.3f},{:.3f}) render=({:.3f},{:.3f}) "
            "body=({:.3f},{:.3f}) shadow=({:.3f},{:.3f}) camera=({:.3f},{:.3f}) "
            "input=({:.2f},{:.2f}) vel=({:.3f},{:.3f}) state={} frame={} "
            "facing={} character={} obstacle={} hp={}/{} armor={} dead={} "
            "enemies={} projectiles={} blocked=({},{})",
            player_.Position().x, player_.Position().y,
            body.x, body.y,
            body.x, body.y,
            shadow.x, shadow.y,
            camera_.position.x, camera_.position.y,
            movement_input_.direction.x, movement_input_.direction.y,
            player_.Velocity().x, player_.Velocity().y,
            engine::animation::LocomotionStateName(locomotion_animator_.State()),
            frame != nullptr ? frame->sprite_id : "none",
            engine::locomotion::FacingDirectionName(facing_),
            player_characters_.Count() > 0
                ? player_characters_.Active().id.c_str() : "none",
            obstacles_.Variants().empty()
                ? "none" : obstacles_.ActiveVariant().name.c_str(),
            player_health_.hp, player_health_.max_hp,
            player_health_.armor, BooleanText(player_dead_),
            enemies_.AliveCount(), projectiles_.ActiveCount(),
            BooleanText(collision.blocked_x), BooleanText(collision.blocked_y));
        for (std::size_t i = 0; i < enemies_.Enemies().size(); ++i) {
            const auto& enemy = enemies_.Enemies()[i];
            LOG_INFO_APP("enemy[{}]: pos=({:.3f},{:.3f}) hp={}/{} dead={} active={}",
                         i, enemy.position.x, enemy.position.y,
                         enemy.health.hp, enemy.health.max_hp,
                         BooleanText(enemy.health.dead),
                         BooleanText(enemy.active));
        }
    }
}

void Application::ExerciseWindowIfRequested() {
    if (!config_.exercise_window || smoke_start_seconds_ < 0.0) return;

    const double elapsed = NowSeconds() - smoke_start_seconds_;
    if (exercise_stage_ == 0 && elapsed >= 1.0) {
        exercise_stage_ = 1;
        LOG_INFO_APP("Exercise: resize to 1600x900");
        (void)window_.Resize(1600, 900);
    } else if (exercise_stage_ == 1 && elapsed >= 2.0) {
        exercise_stage_ = 2;
        LOG_INFO_APP("Exercise: minimize");
        SDL_MinimizeWindow(window_.GetSDLWindow());
    } else if (exercise_stage_ == 2 && elapsed >= 2.6) {
        exercise_stage_ = 3;
        LOG_INFO_APP("Exercise: restore");
        SDL_RestoreWindow(window_.GetSDLWindow());
    } else if (exercise_stage_ == 3 && elapsed >= 3.4) {
        exercise_stage_ = 4;
        LOG_INFO_APP("Exercise: resize back to 1920x1080");
        (void)window_.Resize(1920, 1080);
    }
}

std::string InteractiveTestInstructions(const std::string& name) {
    if (name == "character_all") {
        return "F6 cycles all 15 recovered characters. Confirm each, move and "
               "attack. Enter finalizes, Esc aborts.";
    }
    if (name == "weapon_all") {
        return "F7 selects weapons, Enter equips, 1/2 switches slots, Space "
               "fires. Inspect real projectile and cooldown. Enter finalizes.";
    }
    if (name == "enemy_all") {
        return "F8 selects enemy definitions; F9 spawns one, F11 five, F12 "
               "clears. Inspect real sprites/metadata. Enter finalizes.";
    }
    if (name == "projectile_all") {
        return "F5 opens the asset lab; cycle to PROJECTILES with [ or ], "
               "comma/period cycles items. Enter finalizes.";
    }
    if (name == "object_all" || name == "door_all") {
        return "F5 asset lab: cycle TEXTURES/SPRITES/OBSTACLES/DOORS with "
               "[ or ]. O opens doors, C closes them. Enter finalizes.";
    }
    if (name == "combat_live") {
        return "F9 spawns one enemy, F11 five. Slimes pursue and contact-damage "
               "the player. Space fires; damage, death and R reset are live. "
               "Enter finalizes.";
    }
    if (name == "two_weapon" || name == "test-two-weapons") {
        return "Press 1 for primary, 2 for secondary; each slot keeps its own "
               "weapon, projectile, cooldown and energy cost. Fire both. Enter "
               "finalizes.";
    }
    if (name == "energy") {
        return "Switch to slot 2 (weapon_002 consumes 1) and fire. HUD ENERGY "
               "must decrease per shot and stop firing at zero. Enter finalizes.";
    }
    if (name == "health") {
        return "Let an enemy contact-hit you; HUD HP/ARMOR must change. "
               "Reset with R. Enter finalizes.";
    }
    if (name == "test-ui" || name == "test-font") {
        return "Inspect the 1920x1080 pixel UI: pixel_bold text, HUD panel,\n"
               "buttons and crisp nearest-neighbour scaling. Pause with P. " "Enter finalizes.";
    }
    if (name == "test-player-animation" || name == "test-character-catalogue") {
        return "F5 asset lab: cycle CHARACTERS and ANIMATIONS. Verify idle, run\n"
               "and dead clips, frame stepping and sample rate. Enter finalizes.";
    }
    if (name == "test-player-obstacle-collision") {
        return "Walk around every recovered obstacle from all sides. F4 shows\n"
               "visual/collision/feet/sort debug bounds. Enter finalizes.";
    }
    if (name == "test-weapon-pickup" || name == "test-no-weapon-render") {
        return "The player starts with no weapon and a weapon pickup nearby.\n"
               "Confirm no weapon is rendered and 'E PICK UP' appears; press E " "and confirm the real weapon appears. Enter finalizes.";
    }
    if (name == "test-two-weapons" || name == "two_weapon") {
        return "Equip two weapons, switch with 1/2/Q, fire each, and check\n"
               "that each keeps its own texture/projectile/cooldown. Enter finalizes.";
    }
    if (name == "test-energy") {
        return "Equip weapon_002 (1 energy/shot) and fire; verify the HUD\n"
               "energy value decreases. Enter finalizes.";
    }
    if (name == "test-hud") {
        return "Inspect the top-left HP/armor/energy panel and weapon slots.\n"
               "Take contact damage and verify live values. Enter finalizes.";
    }
    if (name == "test-pause") {
        return "Press P; verify gameplay freezes and the recovered-style pause\n"
               "panel appears. Navigate with arrows, Enter selects, Esc resumes. " "Enter finalizes after resuming.";
    }
    if (name == "test-settings") {
        return "P -> SETTINGS. Toggle Fullscreen and Debug Overlay; verify the\n"
               "pixel UI and that Esc returns to Pause. Enter finalizes.";
    }
    if (name == "test-home") {
        return "From the main menu, navigate NEW GAME / SETTINGS / QUIT and\n"
               "verify HOME from pause returns here. No social tab exists. " "Enter finalizes.";
    }
    if (name == "test-living-room") {
        return "Walk the recovered Living Room, collision-test furniture,\n"
               "use E at the character station and forge, and inspect garden\n"
               "plots. Enter finalizes.";
    }
    if (name == "test-character-select") {
        return "Walk to the character station, press E, browse characters and\n"
               "confirm one; verify its real body/animations and stats. " "Enter finalizes.";
    }
    if (name == "save_load") {
        return "Move, switch weapons, spend energy, then F2 save and F3 load. "
               "Verify character/weapons/HP/energy/position restore. Enter "
               "finalizes.";
    }
    if (name == "room_transition" || name == "test-room-transition") {
        return "Press T (or E at the hub door / an open recovered door) to "
               "switch between the Living Room and r1_1. Verify character, "
               "weapons, HP, armor and energy survive the transition. Enter "
               "finalizes.";
    }
    if (name == "hub") {
        return "HUB IS UNRECOVERED. No hub scene/interactables exist in this "
               "build. Press Enter to record the missing-feature result; Esc "
               "aborts.";
    }
    return "Exercise the devroom controls. Enter finalizes, Esc aborts. Use "
           "--asset-lab for preview-only inspection.";
}

bool Application::LoadReplayIfRequested() {
    replay_ = std::make_unique<soulknight::game::test::InteractiveReplay>();
    std::string error;
    if (!replay_->Load(config_.replay_path, &error)) {
        LOG_FATAL_APP("Replay load failed: {}", error);
        replay_.reset();
        return false;
    }
    LOG_INFO_APP("Replay loaded: {} inputs from {} (test={} commit={})",
                 replay_->InputCount(), replay_->Path().string(),
                 replay_->TestName(), replay_->Commit());
    return true;
}

void Application::StartReplaySession() {
    if (replay_ == nullptr) return;
    const std::string name = "replay_" + replay_->TestName();
    const std::string instructions =
        "Deterministic replay of " + std::to_string(replay_->InputCount()) +
        " recorded fixed-tick inputs. Movement, aim, attack, weapon-slot and "
        "interact actions are replayed together with serialized key commands "
        "(menus, spawners, doors, reset, save/load).";
    const std::filesystem::path output = project_root_ / "test-results";
    if (interactive_test_.Start(name, instructions, LITEKNIGHT_VERSION,
                                SOUL_KNIGHT_COMMIT_HASH, scene_.id,
                                scene_sha256_, output)) {
        interactive_test_.SetAssetHashes(reference_apk_sha256_,
                                         asset_inventory_sha256_,
                                         scene_sha256_);
        LOG_INFO_APP("==================================================");
        LOG_INFO_APP("LiteKnight Interactive Replay");
        LOG_INFO_APP("==================================================");
        LOG_INFO_APP("Source: {}", replay_->Path().string());
        LOG_INFO_APP("Inputs: {}  first tick: {}", replay_->InputCount(),
                     replay_->FirstTick());
        LOG_INFO_APP("Recording replay result under {}", output.string());
    }
}

bool Application::ApplyReplayInputForNextTick() {
    if (replay_ == nullptr) return false;
    const std::uint64_t next_tick = simulation_ticks_ + 1;
    for (const game::test::InteractiveReplay::Command& command :
         replay_->CommandsForTick(next_tick)) {
        HandleSceneKeyDown(static_cast<SDL_Scancode>(command.scancode),
                           static_cast<SDL_Keymod>(command.modifiers));
    }
    if (!replay_->Next(&replay_snapshot_)) return false;
    if (replay_snapshot_.IsPressed(platform::Action::WeaponSlot1)) {
        SwitchWeaponSlot(0);
    }
    if (replay_snapshot_.IsPressed(platform::Action::WeaponSlot2)) {
        SwitchWeaponSlot(1);
    }
    if (replay_snapshot_.IsPressed(platform::Action::Interact)) {
        if (!TryPickupWeapon()) TryInteract();
    }
    ApplyInputSnapshot(replay_snapshot_);
    return true;
}

void Application::ApplyInputSnapshot(const platform::InputSnapshot& snapshot) {
    movement_input_ = scene_mode_ && !dev_ui_.MenuOpen() &&
                     !asset_lab_.IsOpen() && !mech_station_open_ &&
                     !suggestion_edit_mode_ && !calibration_mode_
        ? platform::ReadDigitalMovement(snapshot)
        : platform::MovementInput{};
    if (!scene_mode_) return;
    // NATIVE TEST BINDING: the reference uses a mobile attack button
    // (RGJoyControl.UpdateBtnAtkClick -> RGController.BtnAtkClick).
    // Keyboard Space/J and left mouse are the native dev equivalents.
    attack_held_ = !dev_ui_.MenuOpen() && !asset_lab_.IsOpen() &&
                   !mech_station_open_ && !suggestion_edit_mode_ &&
                   !calibration_mode_ &&
                   (snapshot.IsHeld(platform::Action::Attack) ||
                    snapshot.mouse_pressed[0]);
    UpdateAimInput(&snapshot);
}

bool Application::AdvanceDungeonLevel() {
    if (!biome_.Advance()) return false;
    // New deterministic layout per level. The base seed remains reproducible;
    // the level index is mixed in exactly once.
    map_seed_ = config_.map_seed * 31 +
        static_cast<std::int32_t>(biome_.CurrentIndex()) * 7919;
    room_enemy_snapshots_.clear();
    cleared_rooms_.clear();
    chest_spawned_rooms_.clear();
    item_world_.Reset();
    stored_rooms_.erase(
        std::remove_if(stored_rooms_.begin(), stored_rooms_.end(),
                       [&](const StoredRoom& room) {
                           return room.id != world_hub_id_;
                       }),
        stored_rooms_.end());
    const DungeonPlan plan = BuildDungeonPlan(
        map_seed_, world_templates_,
        biome_.Current() != nullptr ? biome_.Current()->map_long : 3);
    const auto load_room = [&](const std::string& id,
                               const std::string& path) -> bool {
        if (id == scene_.id || path == config_.scene_path) return true;
        for (const StoredRoom& stored : stored_rooms_) {
            if (stored.id == id) return true;
        }
        StoredRoom stored;
        std::string error;
        if (!engine::assets::SceneLoader::Load(filesystem_, path,
                                               stored.scene, &error)) {
            LOG_WARN_APP("Next-level room unavailable: {} ({})", path, error);
            return false;
        }
        stored.id = stored.scene.id;
        if (item_assets_.IsLoaded()) item_assets_.AppendToScene(stored.scene);
        stored_rooms_.push_back(std::move(stored));
        return true;
    };
    for (const Room& room : plan.rooms) {
        if (!load_room(room.id, room.scene_path)) return false;
    }
    // Re-append item assets/unique texture ids for the freshly loaded rooms.
    std::vector<engine::rendering::TextureUpload> uploads;
    for (std::size_t i = 0; i < stored_rooms_.size(); ++i) {
        AssignUniqueTextureIds(
            stored_rooms_[i].scene,
            0x01000000u * (texture_generation_ + 1) +
                static_cast<std::uint32_t>(i) * 0x10000u);
        for (const engine::assets::TextureAsset& texture :
             stored_rooms_[i].scene.textures) {
            uploads.push_back(engine::rendering::TextureUpload{
                texture.id, &texture.data, texture.filter, texture.wrap});
        }
    }
    if (renderer_ != nullptr && !uploads.empty() &&
        !renderer_->AddTextures(uploads)) {
        LOG_ERROR_APP("Next-level texture upload failed");
        return false;
    }
    ConfigureWorldGraph();
    if (!ActivateRoom(plan.start_room_id, "")) return false;
    BeginRoomTitle(3.0f);
    return true;
}

bool Application::LoadWorldRooms(std::int32_t seed_override) {
    world_templates_ = {};
    // Every generated recovered PatternRoom present in the asset tree joins
    // the normal-room pool. MapManagerLevel.GetRandomRoomIndex draws unique
    // template indices; the plan keeps that no-repeat rule.
    for (const auto& entry : filesystem_.ListDirectory("scenes")) {
        const std::string id = entry.filename().string();
        if (id.rfind("r1_", 0) != 0 ||
            id.size() < 11 ||
            id.substr(id.size() - 7) != "_visual") {
            continue;
        }
        const std::string path = "scenes/" + id + "/scene.json";
        if (filesystem_.Exists(path)) {
            world_templates_.normals.push_back({id, path});
        }
    }
    const auto add_room = [&](const char* id,
                              std::vector<soulknight::game::room::DungeonRoomTemplate>* pool,
                              int weight = 1) {
        const std::string scene_id = std::string(id) + "_visual";
        const std::string path = "scenes/" + scene_id + "/scene.json";
        if (filesystem_.Exists(path)) {
            pool->push_back({scene_id, path, weight});
            return true;
        }
        return false;
    };
    add_room("r_start_no_event", &world_templates_.starts);
    add_room("r_transgete", &world_templates_.ends);
    add_room("r1_boss1", &world_templates_.boss_ends);
    add_room("r2_boss1", &world_templates_.boss_ends);
    add_room("r3_boss2", &world_templates_.boss_ends);
    add_room("r_chest", &world_templates_.chests);
    add_room("r_weapon_provider", &world_templates_.specials, 10);
    add_room("r_statue", &world_templates_.specials, 10);
    add_room("r_mineral", &world_templates_.specials, 10);
    add_room("r_mercenary", &world_templates_.specials, 10);
    add_room("r_mount", &world_templates_.specials, 4);
    add_room("r_sell", &world_templates_.specials, 10);
    multi_room_world_ = false;
    const auto known_scene = [&](const std::string& path) {
        if (path == world_hub_path_) return true;
        for (const auto* pool : {&world_templates_.starts,
                                 &world_templates_.normals,
                                 &world_templates_.ends,
                                 &world_templates_.boss_ends,
                                 &world_templates_.chests,
                                 &world_templates_.specials}) {
            for (const auto& room : *pool) {
                if (room.scene_path == path) return true;
            }
        }
        return false;
    };
    if (!known_scene(config_.scene_path)) {
        LOG_INFO_APP("Custom scene mode: multi-room world disabled");
        return false;
    }
    map_seed_ = seed_override == (-2147483647 - 1)
        ? config_.map_seed : seed_override;
    soulknight::game::room::DungeonTemplateSet plan_templates =
        world_templates_;
    const std::string level_id =
        biome_.Current() != nullptr ? biome_.Current()->id : std::string{};
    const bool boss_stage = !level_id.empty() &&
        (level_id.ends_with("-5") || level_id == "3-6");
    if (boss_stage && !plan_templates.boss_ends.empty()) {
        const char chapter = level_id.empty() ? '1' : level_id.front();
        bool selected = false;
        for (const auto& boss : world_templates_.boss_ends) {
            if (!boss.id.empty() && boss.id.front() == 'r' &&
                boss.id.size() > 1 && boss.id[1] == chapter) {
                plan_templates.ends = {boss};
                selected = true;
                break;
            }
        }
        if (!selected) plan_templates.ends = world_templates_.boss_ends;
    }
    const DungeonPlan plan = BuildDungeonPlan(
        map_seed_, plan_templates,
        biome_.Current() != nullptr ? biome_.Current()->map_long : 3);

    const auto load_room = [&](const std::string& id,
                               const std::string& path) -> bool {
        if (path == config_.scene_path) return true;
        for (const StoredRoom& stored : stored_rooms_) {
            if (stored.id == id) return true;
        }
        StoredRoom stored;
        std::string error;
        if (!engine::assets::SceneLoader::Load(filesystem_, path,
                                               stored.scene, &error)) {
            LOG_WARN_APP("Dungeon room unavailable: {} ({})", path, error);
            return false;
        }
        stored.id = stored.scene.id;
        stored_rooms_.push_back(std::move(stored));
        LOG_INFO_APP("Dungeon room loaded: {} ({})", stored_rooms_.back().id,
                     path);
        return true;
    };
    if (!load_room(world_hub_id_, world_hub_path_)) {
        LOG_WARN_APP("Living Room unavailable for multi-room world");
        return false;
    }
    for (const Room& room : plan.rooms) {
        if (!load_room(room.id, room.scene_path)) {
            LOG_WARN_APP("Dungeon graph incomplete without {}", room.id);
            return false;
        }
    }
    multi_room_world_ = true;
    ConfigureWorldGraph();
    return true;
}

void Application::ConfigureWorldGraph() {
    if (world_templates_.normals.empty() && world_templates_.starts.empty()) {
        return;
    }
    const DungeonPlan plan = BuildDungeonPlan(
        map_seed_, world_templates_,
        biome_.Current() != nullptr ? biome_.Current()->map_long : 3);
    std::vector<Room> rooms;
    rooms.push_back(Room{world_hub_id_, world_hub_path_, 0, 1});
    for (const Room& room : plan.rooms) rooms.push_back(room);
    std::vector<RoomConnection> connections = plan.connections;
    connections.push_back(RoomConnection{
        world_hub_id_, plan.start_room_id, "bidirectional",
        "hub_door", "south"});
    room_manager_.Configure(std::move(rooms), std::move(connections));
    room_manager_.SetCurrent(scene_.id);
    world_start_room_id_ = plan.start_room_id;
    room_stage_labels_.clear();
    // CONFIRMED MapManagerLevel 1-1 uses map_long=3. The displayed room code
    // follows BFS distance from the generated start room; the level prefix is
    // the recovered MapManagerLevel.level value.
    std::unordered_map<std::string, int> depth;
    std::vector<std::string> queue;
    depth[world_start_room_id_] = 0;
    queue.push_back(world_start_room_id_);
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const std::string current = queue[head];
        for (const RoomConnection& connection : room_manager_.Connections()) {
            std::string next;
            if (connection.from == current) next = connection.to;
            else if (connection.kind == "bidirectional" &&
                     connection.to == current) next = connection.from;
            if (next.empty() || next == world_hub_id_ ||
                depth.contains(next)) {
                continue;
            }
            depth[next] = depth[current] + 1;
            queue.push_back(next);
        }
    }
    const std::string level_label = biome_.Current() != nullptr
        ? biome_.Current()->id : "1-1";
    for (const std::string& id : queue) {
        if (id == world_hub_id_) continue;
        room_stage_labels_[id] = level_label;
        LOG_INFO_APP("Room code: {} -> {}", id, room_stage_labels_[id]);
    }
    room_graph_text_.clear();
    for (const RoomConnection& connection : room_manager_.Connections()) {
        if (!room_graph_text_.empty()) room_graph_text_ += " | ";
        room_graph_text_ += connection.from + ":" +
            (connection.from_door.empty() ? "logical" : connection.from_door) +
            " <-> " + connection.to + ":" +
            (connection.to_door.empty() ? "logical" : connection.to_door);
    }
    LOG_INFO_APP("Map seed {}: {} rooms, {} physical connections, start {}",
                 map_seed_, room_manager_.Rooms().size(),
                 room_manager_.Connections().size(), plan.start_room_id);
    for (const RoomConnection& connection : room_manager_.Connections()) {
        LOG_INFO_APP("  room edge {}:{} <-> {}:{} ({})", connection.from,
                     connection.from_door.empty() ? "logical" : connection.from_door,
                     connection.to,
                     connection.to_door.empty() ? "logical" : connection.to_door,
                     connection.kind);
    }
}

engine::Vec2 Application::DoorEntrancePosition(
    const engine::assets::SceneAsset& scene, const std::string& door_id) const {
    if (door_id.empty()) return scene.spawn;
    if (scene.is_hub) {
        for (const engine::assets::InteractionMarker& marker :
             scene.interaction_markers) {
            if (marker.kind == "door") {
                return {marker.position.x, marker.position.y - 2.2f};
            }
        }
        return scene.spawn;
    }
    const int32_t index = scene.FindDoorIndex(door_id);
    if (index < 0) return scene.spawn;
    const engine::assets::SceneDoor& door =
        scene.doors[static_cast<std::size_t>(index)];
    engine::Vec2 position{
        (door.trigger.min.x + door.trigger.max.x) * 0.5f,
        (door.trigger.min.y + door.trigger.max.y) * 0.5f};
    constexpr float kEntranceInset = 1.35f;
    if (door.id == "north") {
        position.y = door.trigger.min.y - kEntranceInset;
    } else if (door.id == "south") {
        position.y = door.trigger.max.y + kEntranceInset;
    } else if (door.id == "east") {
        position.x = door.trigger.min.x - kEntranceInset;
    } else if (door.id == "west") {
        position.x = door.trigger.max.x + kEntranceInset;
    }
    return position;
}

const engine::assets::SceneDoor* Application::NearbyOpenDoor() const {
    if (scene_.is_hub || scene_.doors.empty()) return nullptr;
    const engine::assets::SceneDoor* best = nullptr;
    float best_distance = 4.0f;
    for (std::size_t i = 0; i < scene_.doors.size() &&
                            i < room_doors_.Count(); ++i) {
        if (room_doors_.At(i).IsBlocking()) continue;
        const auto& trigger = scene_.doors[i].trigger;
        const engine::Vec2 center{
            (trigger.min.x + trigger.max.x) * 0.5f,
            (trigger.min.y + trigger.max.y) * 0.5f};
        const float distance = (center - player_.Position()).LengthSquared();
        if (distance <= best_distance) {
            best_distance = distance;
            best = &scene_.doors[i];
        }
    }
    return best;
}

bool Application::NearOpenDoor() const {
    return NearbyOpenDoor() != nullptr;
}

std::string Application::FirstConnectedDoor() const {
    if (scene_.is_hub) return "hub_door";
    for (const RoomConnection& connection : room_manager_.Connections()) {
        if (connection.from == scene_.id && !connection.from_door.empty()) {
            return connection.from_door;
        }
        if (connection.to == scene_.id && !connection.to_door.empty() &&
            connection.kind == "bidirectional") {
            return connection.to_door;
        }
    }
    return {};
}

void Application::BeginRoomTitle(float seconds) {
    room_title_timer_ = std::max(0.0f, seconds);
}

void Application::SealUnconnectedDoors() {
    if (!multi_room_world_ || room_manager_.Current() == nullptr) return;
    for (std::size_t i = 0; i < room_doors_.Count(); ++i) {
        const std::string& id = room_doors_.At(i).Data().id;
        if (room_manager_.ConnectionFromDoor(id) == nullptr) {
            room_doors_.SealDoor(i);
            LOG_INFO_APP("Door sealed (no map edge): {}", id);
        }
    }
}

void Application::UpdateRoomClear() {
    if (!room_clear_pending_ || scene_.is_hub) return;
    // Boss rooms are type 2 but need their recovered boss chest after clear.
    const bool boss_room = scene_.id.rfind("r1_boss", 0) == 0 ||
                           scene_.id.rfind("r2_boss", 0) == 0 ||
                           scene_.id.rfind("r3_boss", 0) == 0;
    if (scene_.room.type != 1 && !boss_room) return;
    if (enemies_.AliveCount() != 0) return;
    room_clear_pending_ = false;
    cleared_rooms_.insert(scene_.id);
    room_activation_state_ =
        soulknight::game::room::RoomActivationState::Cleared;
    room_clear_timer_ = 1.5f;
    room_doors_.OpenAll();
    if (!scene_.is_hub && !chest_spawned_rooms_.contains(scene_.id)) {
        const std::string chest_id =
            boss_room ? "chest_boss" : current_chest_definition_;
        item_world_.AddChest(chest_id,
                             scene_.spawn + engine::Vec2{0.0f, 2.4f});
        chest_spawned_rooms_.insert(scene_.id);
        LOG_INFO_APP("Room clear: {} chest appeared ({})",
                     boss_room ? "boss" : "silver", chest_id);
    }
    RebuildCollisionWorld();
    BeginRoomTitle(3.0f);
    LOG_INFO_APP("Room clear: all enemies defeated; doors opened");
}

void Application::CaptureCurrentRoomState() {
    if (scene_.id.empty()) return;
    room_enemy_snapshots_[scene_.id] = enemies_.Enemies();
    if (!scene_.is_hub &&
        (enemies_.AliveCount() == 0 || !room_clear_pending_)) {
        cleared_rooms_.insert(scene_.id);
    }
    LOG_INFO_APP("Room state captured: {} clear={} enemies={}", scene_.id,
                 BooleanText(cleared_rooms_.contains(scene_.id)),
                 enemies_.Enemies().size());
}

void Application::RestoreCurrentRoomState() {
    const auto snapshot = room_enemy_snapshots_.find(scene_.id);
    if (snapshot != room_enemy_snapshots_.end()) {
        enemies_.RestoreEnemies(snapshot->second);
    }
}

bool Application::SwitchRoom(const std::string& requested_door) {
    if (!multi_room_world_) return false;
    std::string door_id = requested_door;
    if (door_id.empty()) {
        if (scene_.is_hub) {
            door_id = "hub_door";
        } else if (const engine::assets::SceneDoor* door = NearbyOpenDoor()) {
            door_id = door->id;
        } else {
            door_id = FirstConnectedDoor();
        }
    }
    if (door_id.empty()) {
        LOG_WARN_APP("Room transition: no connected door in {}", scene_.id);
        return false;
    }
    CaptureCurrentRoomState();
    std::string destination_door;
    if (!room_manager_.TransitionThroughDoor(door_id, &destination_door)) {
        LOG_WARN_APP("Room transition rejected: {} via door {}",
                     scene_.id, door_id);
        return false;
    }
    const Room* target = room_manager_.Current();
    if (target == nullptr) return false;
    LOG_INFO_APP("Physical room transition: {} door {} -> {} door {}",
                 scene_.id, door_id, target->id,
                 destination_door.empty() ? "logical" : destination_door);
    return ActivateRoom(target->id, destination_door);
}

bool Application::ActivateRoom(const std::string& room_id,
                               const std::string& entrance_door) {
    if (room_id == scene_.id) return true;
    StoredRoom* target = nullptr;
    for (StoredRoom& stored : stored_rooms_) {
        if (stored.id == room_id) {
            target = &stored;
            break;
        }
    }
    if (target == nullptr) {
        LOG_ERROR_APP("Room activation failed: unknown room {}", room_id);
        return false;
    }
    const std::string character_id =
        player_characters_.Count() > 0 ? player_characters_.Active().id : "c00";
    const std::size_t slot0 =
        weapon_inventory_.SlotOccupied(0) ? weapon_inventory_.Slot(0)
                                          : weapons_.Count();
    const std::size_t slot1 =
        weapon_inventory_.SlotOccupied(1) ? weapon_inventory_.Slot(1)
                                          : weapons_.Count();
    const std::string weapon0 = slot0 < weapons_.Count()
        ? weapons_.Definitions()[slot0].id : std::string{};
    const std::string weapon1 = slot1 < weapons_.Count()
        ? weapons_.Definitions()[slot1].id : std::string{};
    const std::size_t active_slot = weapon_inventory_.ActiveSlot();
    const int hp = player_health_.hp;
    const int armor = player_health_.armor;
    const int energy = player_energy_.energy;
    const bool dead = player_health_.dead;

    const std::string previous_id = scene_.id;
    std::swap(scene_, target->scene);
    target->id = previous_id;
    room_manager_.SetCurrent(scene_.id);

    std::string error;
    if (!player_characters_.Configure(scene_, &error)) {
        LOG_ERROR_APP("Room switch character setup failed: {}", error);
        return false;
    }
    std::size_t character_index = 0;
    for (std::size_t i = 0; i < player_characters_.Count(); ++i) {
        if (player_characters_.CharacterAt(i).id == character_id) {
            character_index = i;
            break;
        }
    }
    player_characters_.Select(character_index);
    game_ui_.SetCharacterCount(static_cast<int>(player_characters_.Count()));
    if (!weapons_.Configure(scene_)) {
        LOG_ERROR_APP("Room switch weapon setup failed");
        return false;
    }
    game_ui_.SetWeaponCount(static_cast<int>(weapons_.Count()));
    weapon_inventory_.Configure(weapons_.Count());
    const auto equip_by_id = [&](std::size_t slot, const std::string& id) {
        if (id.empty()) return;
        for (std::size_t i = 0; i < weapons_.Count(); ++i) {
            if (weapons_.Definitions()[i].id == id) {
                weapon_inventory_.SwitchSlot(slot);
                (void)weapon_inventory_.EquipActive(i);
                break;
            }
        }
    };
    equip_by_id(0, weapon0);
    equip_by_id(1, weapon1);
    if (!weapon_inventory_.SlotOccupied(0) && weapons_.Count() > 0) {
        weapon_inventory_.SwitchSlot(0);
        std::size_t baseline = 0;
        if (player_characters_.Count() > 0) {
            for (std::size_t i = 0; i < weapons_.Count(); ++i) {
                if (weapons_.Definitions()[i].id ==
                    player_characters_.Active().starting_weapon) {
                    baseline = i;
                    break;
                }
            }
        }
        (void)weapon_inventory_.EquipActive(baseline);
    }
    weapon_inventory_.SwitchSlot(active_slot);
    ConfigureActiveWeapon();
    ConfigureSecondaryWeapon();
    ConfigureAbilities();

    const engine::Vec2 spawn = DoorEntrancePosition(scene_, entrance_door);
    player_ = engine::locomotion::PlayerLocomotion(spawn);
    previous_player_position_ = player_.Position();
    render_player_position_ = player_.Position();
    camera_ = {};
    camera_.position = spawn;
    camera_.orthographic_size = scene_.camera.orthographic_size > 0.0f
        ? scene_.camera.orthographic_size : 10.0f;
    ConfigureCameraBounds();
    if (camera_bounds_valid_) {
        camera_.ClampToRect(camera_bounds_.min, camera_bounds_.max);
    }
    previous_camera_position_ = camera_.position;
    render_camera_ = camera_;
    player_health_.Reset(
        player_characters_.Active().max_hp > 0
            ? player_characters_.Active().max_hp : 6,
        player_characters_.Active().max_armor > 0
            ? player_characters_.Active().max_armor : 5, 0);
    player_health_.hp = std::min(hp, player_health_.max_hp);
    player_health_.armor = std::min(armor, player_health_.max_armor);
    if (scene_.is_hub) {
        player_health_.hp = player_health_.max_hp;
        player_health_.armor = player_health_.max_armor;
    }
    player_health_.dead = dead || player_health_.hp <= 0;
    player_health_.can_hurt = !player_health_.dead;
    player_energy_.Reset(
        player_characters_.Active().max_energy > 0
            ? player_characters_.Active().max_energy : 180);
    player_energy_.energy = std::min(energy, player_energy_.max_energy);
    if (scene_.is_hub) player_energy_.energy = player_energy_.max_energy;
    player_dead_ = player_health_.dead;
    player_.SetSpeedProfile(
        player_characters_.Active().speed > 0.0f
            ? player_characters_.Active().speed : 6.5f,
        player_characters_.Active().speed_rate);
    locomotion_animator_.Configure(player_characters_.IdleClip(),
                                   player_characters_.RunClip(),
                                   player_characters_.DeathClip(),
                                   player_characters_.HitClip());
    if (player_dead_) locomotion_animator_.PlayDead();
    last_animation_sprite_.clear();
    last_animation_state_ = engine::animation::LocomotionState::Idle;
    animation_logs_remaining_ = 8;
    player_render_ = engine::rendering::BindPlayerRenderInstances(scene_);
    if (!player_render_.IsValid()) {
        LOG_ERROR_APP("Room switch player render binding missing");
        return false;
    }
    const bool had_enemy_snapshot =
        room_enemy_snapshots_.contains(scene_.id);
    std::string enemy_error;
    if (!enemies_.Configure(scene_, &enemy_error)) {
        LOG_ERROR_APP("Room switch enemy setup failed: {}", enemy_error);
        return false;
    }
    RestoreCurrentRoomState();
    (void)obstacles_.Configure(scene_, &error);
    boxes_.Configure(scene_, obstacles_);
    ConfigureRoomItems();
    std::string pet_error;
    if (pet_.Configure(scene_, &pet_error)) {
        if (!active_pet_id_.empty()) pet_.SelectSaved(active_pet_id_);
        active_pet_id_ = pet_.SelectedId();
    }
    training_stake_.Configure(scene_);
    BeginRoomTitle(3.0f);
    room_transition_flash_ = 0.45f;
    if (!room_doors_.Configure(scene_, &error)) {
        LOG_ERROR_APP("Room switch door setup failed: {}", error);
        return false;
    }
    SealUnconnectedDoors();
    door_trigger_inside_.assign(room_doors_.Count(), false);
    room_clear_pending_ = false;
    if (!scene_.is_hub && !scene_.doors.empty()) {
        if (cleared_rooms_.contains(scene_.id) || enemies_.AliveCount() == 0) {
            room_doors_.OpenAll();
            cleared_rooms_.insert(scene_.id);
            LOG_INFO_APP("Room clear: {} is already clear; doors open", scene_.id);
        } else {
            room_doors_.CloseAll();
            room_clear_pending_ = true;
            LOG_INFO_APP("Room clear: doors closed until enemies are defeated");
        }
    }
    RebuildCollisionWorld();
    if (!had_enemy_snapshot && !scene_.is_hub) PopulateBiomeEnemies();
    room_entry_position_ = player_.Position();
    room_activation_timer_ = 0.0f;
    mech_station_open_ = false;
    if (scene_.is_hub) {
        room_activation_state_ =
            soulknight::game::room::RoomActivationState::Inactive;
    } else if (!cleared_rooms_.contains(scene_.id) &&
               enemies_.AliveCount() > 0) {
        room_activation_state_ =
            soulknight::game::room::RoomActivationState::Entered;
    } else {
        room_activation_state_ =
            soulknight::game::room::RoomActivationState::Cleared;
    }
    LOG_INFO_APP("Room activation: {} state={}", scene_.id,
                 soulknight::game::room::RoomActivationName(
                     room_activation_state_));
    projectiles_.Clear();
    weapon_pickups_.Clear();
    if (scene_.is_hub) ConfigureHubGarden();
    window_.SetTitle(scene_.is_hub
        ? "LiteKnight Living Room"
        : std::string("LiteKnight Devroom - ") + scene_.id);
    if (std::find(visited_rooms_.begin(), visited_rooms_.end(), scene_.id) ==
        visited_rooms_.end()) {
        visited_rooms_.push_back(scene_.id);
    }
    LOG_INFO_APP("Room transition: NOW {} entrance={} spawn=({:.2f},{:.2f}) "
                 "hp={} armor={} energy={} char={} weapon={} clear={}",
                 scene_.id, entrance_door.empty() ? "spawn" : entrance_door,
                 spawn.x, spawn.y, player_health_.hp, player_health_.armor,
                 player_energy_.energy, character_id,
                 weapon_.IsConfigured() ? weapon_.Definition().id : "EMPTY",
                 BooleanText(cleared_rooms_.contains(scene_.id)));
    return true;
}

void Application::StartInteractiveTest() {
    const std::string& name = config_.interactive_test_name;
    const std::string instructions = InteractiveTestInstructions(name);
    const std::filesystem::path output = project_root_ / "test-results";
    if (interactive_test_.Start(name, instructions, LITEKNIGHT_VERSION,
                                SOUL_KNIGHT_COMMIT_HASH, scene_.id,
                                scene_sha256_, output)) {
        interactive_test_.SetAssetHashes(reference_apk_sha256_,
                                         asset_inventory_sha256_,
                                         scene_sha256_);
        LOG_INFO_APP("==================================================");
        LOG_INFO_APP("LiteKnight Interactive Test");
        LOG_INFO_APP("==================================================");
        LOG_INFO_APP("Test: {}", name);
        LOG_INFO_APP("Instructions: {}", instructions);
        LOG_INFO_APP("Recording... [ENTER] finalize  [ESC] abort");
        LOG_INFO_APP("Output directory: {}", output.string());
    } else {
        LOG_ERROR_APP("Interactive test could not be started");
    }
}

void Application::RecordInteractiveTestTick() {
    if (!interactive_test_.Active()) return;
    game::test::TestState state;
    state.position = player_.Position();
    state.velocity = player_.Velocity();
    state.camera_x = camera_.position.x;
    state.camera_y = camera_.position.y;
    state.animation_state =
        engine::animation::LocomotionStateName(locomotion_animator_.State());
    const engine::animation::SpriteKeyframe* frame =
        locomotion_animator_.Evaluate("c00/img/body");
    state.animation_frame = frame != nullptr ? frame->sprite_id : "none";
    state.character = player_characters_.Count() > 0
        ? player_characters_.Active().id : "none";
    state.weapon = weapon_.IsConfigured() ? weapon_.Definition().id : "none";
    state.room = scene_.id;
    state.hp = player_health_.hp;
    state.max_hp = player_health_.max_hp;
    state.armor = player_health_.armor;
    state.energy = player_energy_.energy;
    state.max_energy = player_energy_.max_energy;
    state.enemies = enemies_.Enemies().size();
    state.projectiles = projectiles_.ActiveCount();
    state.dead = player_dead_;
    state.doors_open = room_doors_.TargetOpen();
    const platform::InputSnapshot& snapshot =
        replay_ != nullptr ? replay_snapshot_ : input_manager_.GetSnapshot();
    interactive_test_.RecordTick(simulation_ticks_, snapshot, state);
}

void Application::FinalizeInteractiveTest(const std::string& result,
                                          const std::string& notes) {
    if (!interactive_test_.Active()) return;
    if (interactive_test_.Finalize(result, notes)) {
        LOG_INFO_APP("Interactive test {}: {} -> {}", interactive_test_.Name(),
                     result, interactive_test_.OutputPath());
        if (result != "PASS") exit_code_ = 1;
    } else {
        LOG_ERROR_APP("Interactive test {} could not be serialized",
                      interactive_test_.Name());
    }
}

bool Application::SmokeTestExpired() const {
    if (config_.smoke_test_seconds <= 0.0 || smoke_start_seconds_ < 0.0) return false;
    return (NowSeconds() - smoke_start_seconds_) >= config_.smoke_test_seconds;
}

void Application::RequestShutdown() {
    running_ = false;
}

int Application::Run() {
    if (!Initialize()) {
        Shutdown();
        return EXIT_FAILURE;
    }

    bool render_failed = false;
    int frame_log_counter = 0;
    while (running_) {
        PollEvents();
        logical_viewport_.Update(window_.GetPixelWidth(),
                                 window_.GetPixelHeight(),
                                 window_.GetWidth(), window_.GetHeight());
        LogViewportContract();
        if (scene_mode_ && replay_ == nullptr) {
            ApplyInputSnapshot(input_manager_.GetSnapshot());
        } else if (!scene_mode_) {
            movement_input_ = {};
        }
        if (!running_) break;
        if (SmokeTestExpired()) {
            LOG_INFO_APP("Smoke test duration reached");
            break;
        }

        frame_timer_.BeginFrame();

        const bool ui_frozen = game_ui_.IsPaused() || game_ui_.IsMainMenu() ||
                               ui_design_mode_;
        if (ui_frozen) {
            movement_input_ = {};
            attack_held_ = false;
        }
        while (frame_timer_.ShouldSimulate()) {
            if (ui_frozen) {
                frame_timer_.ConsumeSimStep();
                continue;
            }
            if (replay_ != nullptr && !ApplyReplayInputForNextTick()) {
                FinalizeInteractiveTest(
                    "PASS", "replay input stream exhausted: " +
                            replay_->Path().string());
                running_ = false;
                break;
            }
            FixedUpdate(frame_timer_.GetFixedDeltaTime());
            frame_timer_.ConsumeSimStep();
            if (replay_ != nullptr && replay_->Finished()) {
                FinalizeInteractiveTest(
                    "PASS", "replay input stream exhausted: " +
                            replay_->Path().string());
                running_ = false;
                break;
            }
        }

        if (scene_mode_) {
            // Render interpolation: gameplay stays at 50 Hz, but the visual
            // camera/player position is sampled between the previous and
            // current fixed states for a smooth 60 FPS presentation.
            const float alpha = static_cast<float>(
                std::clamp(frame_timer_.GetInterpolationAlpha(), 0.0, 1.0));
            render_player_position_ = previous_player_position_ +
                (player_.Position() - previous_player_position_) * alpha;
            render_camera_ = camera_;
            render_camera_.position = previous_camera_position_ +
                (camera_.position - previous_camera_position_) * alpha;
            render_aim_direction_ = previous_aim_direction_ +
                (aim_direction_ - previous_aim_direction_) * alpha;
            if (render_aim_direction_.LengthSquared() > 1e-6f) {
                render_aim_direction_ = render_aim_direction_.Normalized();
            } else {
                render_aim_direction_ = aim_direction_;
            }
            if (!scene_mode_) {
                render_camera_.position = camera_.position;
                render_player_position_ = player_.Position();
            }
            BuildRenderBatch();
        }

        const auto& stats = frame_timer_.GetStats();
        if (window_.IsMinimized()) {
            // Do not busy-loop or submit frames for a zero-sized/minimized
            // surface. The compositor will send RESTORED when it is ready.
            SDL_Delay(16);
        } else {
            if (!renderer_->Render(sprite_batch_, render_camera_)) {
                LOG_FATAL_APP("Renderer frame failed");
                render_failed = true;
                running_ = false;
            } else {
                ++render_frames_;
                window_.ClearResizeRequest();
                const auto now = std::chrono::steady_clock::now();
                if (warmup_frames_ == 0 && last_present_time_ !=
                    std::chrono::steady_clock::time_point{}) {
                    const double interval_ms =
                        std::chrono::duration<double, std::milli>(
                            now - last_present_time_).count();
                    if (present_window_samples_ == 0) {
                        present_window_min_ms_ = interval_ms;
                        present_window_max_ms_ = interval_ms;
                    } else {
                        present_window_min_ms_ =
                            std::min(present_window_min_ms_, interval_ms);
                        present_window_max_ms_ =
                            std::max(present_window_max_ms_, interval_ms);
                    }
                    present_window_sum_ms_ += interval_ms;
                    ++present_window_samples_;
                    present_run_sum_ms_ += interval_ms;
                    ++present_run_samples_;
                }
                if (warmup_frames_ == 0) last_present_time_ = now;
            }
        }

        frame_timer_.EndFrame();

        if (warmup_frames_ > 0) {
            --warmup_frames_;
        } else {
            ++perf_samples_;
            perf_total_frame_ms_ += stats.frame_time_ms;
            perf_total_sim_steps_ += static_cast<unsigned long long>(stats.sim_steps);
        }

        ++frame_log_counter;
        if (frame_log_counter >= 120) {
            frame_log_counter = 0;
            if (present_window_samples_ > 0) {
                const double avg = present_window_sum_ms_ /
                    static_cast<double>(present_window_samples_);
                LOG_INFO_PERF("frame_ms={:.3f} sim_steps={} render_frames={} "
                              "present_avg_ms={:.3f} present_min_ms={:.3f} "
                              "present_max_ms={:.3f} present_hz={:.2f}",
                              stats.frame_time_ms, stats.sim_steps,
                              render_frames_, avg, present_window_min_ms_,
                              present_window_max_ms_,
                              avg > 0.0 ? 1000.0 / avg : 0.0);
                present_window_sum_ms_ = 0.0;
                present_window_min_ms_ = 0.0;
                present_window_max_ms_ = 0.0;
                present_window_samples_ = 0;
            } else {
                LOG_INFO_PERF("frame_ms={:.3f} sim_steps={} render_frames={}",
                              stats.frame_time_ms, stats.sim_steps,
                              render_frames_);
            }
        }

        ExerciseWindowIfRequested();
    }

    if (perf_samples_ > 0) {
        const double average_frame_ms = perf_total_frame_ms_ / static_cast<double>(perf_samples_);
        const double average_sim_steps = static_cast<double>(perf_total_sim_steps_) /
                                         static_cast<double>(perf_samples_);
        LOG_INFO_PERF("measured average frame: {:.3f} ms over {} frames ({:.3f} sim steps/frame)",
                      average_frame_ms, perf_samples_, average_sim_steps);
    }
    if (present_run_samples_ > 0) {
        const double avg = present_run_sum_ms_ /
            static_cast<double>(present_run_samples_);
        LOG_INFO_APP("Present pacing: avg {:.3f} ms ({:.2f} Hz) over {} intervals",
                     avg, avg > 0.0 ? 1000.0 / avg : 0.0,
                     present_run_samples_);
    }
    LOG_INFO_APP("Rendered {} frames, advanced {} fixed simulation ticks ({:.3f} s sim time)",
                 render_frames_, simulation_ticks_, simulation_time_);

    if (interactive_test_.Active()) {
        FinalizeInteractiveTest("ABORTED", "application exited before finalization");
    }
    running_ = false;
    Shutdown();
    return (render_failed || exit_code_ != 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

void Application::Shutdown() {
    if (shutdown_complete_) return;
    shutdown_complete_ = true;
    running_ = false;

    LOG_INFO_APP("Application shutdown");

    audio_voices_.clear();
    music_ = platform::AudioEngine::SoundHandle{};
    audio_engine_.Shutdown();

    if (renderer_) {
        renderer_->Shutdown();
        renderer_.reset();
    }

    input_manager_.Shutdown();
    window_.Shutdown();

    if (vulkan_library_loaded_) {
        SDL_Vulkan_UnloadLibrary();
        vulkan_library_loaded_ = false;
    }

    if (sdl_initialized_) {
        SDL_Quit();
        sdl_initialized_ = false;
    }

    initialized_ = false;
    LOG_INFO_APP("Shutdown complete");
}

} // namespace soulknight::app
