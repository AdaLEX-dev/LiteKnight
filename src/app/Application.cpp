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
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>

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
            // CONFIRMED baseline: c00's WeaponInfo entry carries the
            // InitWeapon tag and is weapon_000 (Bad Pistol). The reference
            // GetInitWeaponName path installs it before the first room.
            if (player_characters_.Count() > 0 &&
                player_characters_.Active().id == "c00" &&
                weapons_.Count() > 0) {
                (void)weapon_inventory_.EquipActive(0);
            }
            ConfigureActiveWeapon();
            ConfigureSecondaryWeapon();
            ConfigureAbilities();
            ConfigureTraderOffers();
            weapon_pickups_.Clear();
        }
        LOG_INFO_APP("Combat: player hp={} armor={} camp={}; enemies={} "
                     "(definitions={}), projectiles=0",
                     player_health_.hp, player_health_.armor,
                     player_health_.camp, enemies_.Enemies().size(),
                     enemies_.Definitions().size());
        if (scene_.is_hub) {
            plant_system_.Clear();
            plant_system_.AddPlot("garden_1", {-27.0f, -2.5f}, 5.0f);
            plant_system_.AddPlot("garden_2", {-25.0f, -2.5f}, 5.0f);
            plant_system_.AddPlot("garden_3", {-27.0f, 0.0f}, 5.0f);
            plant_system_.AddPlot("garden_4", {-25.0f, 0.0f}, 5.0f);
        }
        if (config_.asset_lab) {
            asset_lab_.Open();
        }
        ConfigureRoomItems();
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

void Application::BuildRenderBatch() {
    sprite_batch_.Clear();
    if (!scene_mode_) {
        BuildSyntheticBatch();
        return;
    }

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
        if (engine::rendering::IsCharacterSortedUnityLayer(
                instance.sorting_layer)) {
            draw.layer = engine::rendering::SortLayer::Character;
            draw.sorting_order = engine::rendering::WorldDepthOrder(
                draw.position.y, is_player_body ? 20 : 0);
        } else {
            draw.layer = MapSortingLayer(instance.sorting_layer);
            draw.sorting_order = instance.sorting_order;
        }
        draw.texture_id = sprite->texture_id;
        draw.pivot = sprite->pivot;
        draw.rotation = instance.rotation;
        draw.flip_x = instance.flip_x;
        if (is_player_body && hit_flash_timer_ > 0.0f) {
            const bool bright = static_cast<int>(hit_flash_timer_ * 100.0f) % 2 == 0;
            draw.color = bright ? engine::Color{255, 245, 235, 255}
                                : engine::Color{255, 150, 150, 255};
        }
        if (is_player_body) {
            draw.flip_x = engine::rendering::ResolvePlayerBodyFlipX(
                instance.flip_x, facing_);
        }
        draw.flip_y = instance.flip_y;
        (void)sprite_batch_.DrawSprite(draw);
    }

    if (!obstacles_.Variants().empty()) {
        const engine::assets::ObstacleVariant& obstacle_variant =
            obstacles_.ActiveVariant();
        for (const engine::assets::ObstacleMarker& marker :
             obstacles_.Markers()) {
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
                draw.color = visual.color;
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

    if (collision_debug_) DrawCollisionDebug();

    if (weapon_.IsConfigured()) {
        soulknight::game::weapons::WeaponRenderer::Draw(
            sprite_batch_, scene_, weapon_.Definition(), player_.Position(),
            render_aim_direction_,
            facing_ == engine::locomotion::FacingDirection::Left,
            player_dead_);
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
                {0.55f * mirror, 0.6f}, player_dead_);
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
    if (scancode == SDL_SCANCODE_N) {
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
    if (scene_.is_hub && scancode == SDL_SCANCODE_H) {
        game_ui_.OpenCharacterSelect(
            static_cast<int>(player_characters_.ActiveIndex()));
        return;
    }
    if (scene_.is_hub && scancode == SDL_SCANCODE_B) {
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
            } else if (scancode == SDL_SCANCODE_F9) {
                SpawnSelectedEnemy((modifiers & SDL_KMOD_SHIFT) != 0 ? 5 : 1);
            } else if (scancode == SDL_SCANCODE_F11) {
                (void)window_.SetFullscreen(!window_.IsFullscreen());
                LOG_INFO_APP("Fullscreen: {}",
                             BooleanText(window_.IsFullscreen()));
            } else if (scancode == SDL_SCANCODE_F12) {
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
            } else if (scancode == SDL_SCANCODE_G) {
                SpawnWeaponPickup(weapons_.ActiveIndex());
            } else if (scancode == SDL_SCANCODE_O) {
                if (!scene_.doors.empty()) {
                    ToggleAllDoors();
                } else {
                    player_orbs_ += 100;
                    LOG_INFO_APP("Developer: +100 orbs (total {})",
                                 player_orbs_);
                }
            } else if (scancode == SDL_SCANCODE_C &&
                       !scene_.doors.empty()) {
                CloseAllDoors();
            } else if (scancode == SDL_SCANCODE_LEFTBRACKET) {
                CycleCharacter(-1);
            } else if (scancode == SDL_SCANCODE_RIGHTBRACKET) {
                CycleCharacter(1);
            } else if (scancode == SDL_SCANCODE_Y) {
                CycleObstacleVariant();
            } else if (scancode == SDL_SCANCODE_R) {
                ResetCombat();
            } else if ((modifiers & SDL_KMOD_CTRL) != 0 &&
                       scancode >= SDL_SCANCODE_1 &&
                       scancode <= SDL_SCANCODE_9) {
                SwitchCharacter(static_cast<std::size_t>(
                    scancode - SDL_SCANCODE_1));
            } else if ((modifiers & SDL_KMOD_CTRL) != 0 &&
                       scancode == SDL_SCANCODE_0) {
                SwitchCharacter(9);
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
        case UIAction::NewGame:
            ResetCombat();
            game_ui_.OpenGameplay();
            break;
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
    previous_camera_position_ = position;
    render_camera_ = camera_;
    LOG_INFO_APP("Review teleport: ({:.2f},{:.2f})", position.x, position.y);
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
               config_.review_area == "chest") {
        game_ui_.OpenGameplay();
    } else if (config_.review_area == "workshop" ||
               config_.review_area == "garden" ||
               config_.review_area == "cellar" ||
               config_.review_area == "trader") {
        game_ui_.OpenGameplay();
        engine::Vec2 target = config_.review_area == "cellar"
            ? engine::Vec2{10.0f, -24.0f}
            : (config_.review_area == "trader"
                ? engine::Vec2{27.6f, 2.2f} : scene_.spawn);
        for (const auto& marker : scene_.interaction_markers) {
            if ((config_.review_area == "workshop" &&
                 marker.kind == "forge") ||
                (config_.review_area == "garden" &&
                 marker.kind == "garden") ||
                (config_.review_area == "cellar" &&
                 marker.kind == "cellar")) {
                target = marker.position + engine::Vec2{0.0f, -2.0f};
                break;
            }
        }
        TeleportForReview(target);
        if (config_.review_area == "trader") {
            game_ui_.OpenTrader();
        }
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
    std::string devroom_room;
    const auto room_scene_for = [&](const std::string& id)
        -> const engine::assets::SceneAsset* {
        if (id == scene_.id) return &scene_;
        for (const StoredRoom& stored : stored_rooms_) {
            if (stored.id == id) return &stored.scene;
        }
        return nullptr;
    };
    for (const auto& room : room_manager_.Rooms()) {
        if (devroom_room.empty() && room.id.rfind("r1_", 0) == 0) {
            devroom_room = room.id;
        }
        std::string icon;
        if (room.id == "hero_room") {
            icon = "home";
        } else if (const engine::assets::SceneAsset* room_scene =
                       room_scene_for(room.id); room_scene != nullptr) {
            switch (room_scene->room.type) {
                case 2: icon = "boss"; break;
                case 3: icon = "special"; break;
                case 4: icon = "chest"; break;
                case 5: icon = "special"; break;
                default: break;
            }
        }
        data.map_rooms.push_back(soulknight::game::ui::MapRoomView{
            room.id, room.map_x, room.map_y, room.id == scene_.id,
            std::find(visited_rooms_.begin(), visited_rooms_.end(), room.id) !=
                visited_rooms_.end(),
            false, icon});
    }
    if (scene_.id.rfind("r1_", 0) == 0) devroom_room = scene_.id;
    for (auto& room : data.map_rooms) room.devroom = room.id == devroom_room;
    data.devroom_room_id = devroom_room;
    data.room_label = scene_.room.label;
    if (!scene_.is_hub) {
        const auto stage = room_stage_labels_.find(scene_.id);
        if (stage != room_stage_labels_.end()) {
            data.room_label = stage->second;
        }
    }
    data.room_clear_label = scene_.room.clear_label;
    data.room_cleared = cleared_rooms_.contains(scene_.id);
    data.room_title_timer = room_title_timer_;
    data.transition_flash = room_transition_flash_;
    if (scene_.is_hub) {
        // Native conceptual nodes for the physically accessible hub areas. The
        // reference is one HeroRoom scene, so these map cells are an explicit
        // visualization of the native subareas, not separate MapManager rooms.
        data.map_rooms.push_back(soulknight::game::ui::MapRoomView{
            "hub_garden", -1, 1, false, true, false, "special"});
        data.map_rooms.push_back(soulknight::game::ui::MapRoomView{
            "hub_workshop", 1, 1, false, true, false, "special"});
        data.map_rooms.push_back(soulknight::game::ui::MapRoomView{
            "hub_cellar", 0, 0, false, true, false, "chest"});
        data.map_links.push_back(soulknight::game::ui::MapLinkView{0, 1, -1, 1});
        data.map_links.push_back(soulknight::game::ui::MapLinkView{0, 1, 1, 1});
        data.map_links.push_back(soulknight::game::ui::MapLinkView{0, 1, 0, 0});
    }
    for (const auto& connection : room_manager_.Connections()) {
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
            data.interaction_label = "RETURN TO LIVING ROOM? E AGAIN";
        }
        data.interaction_screen = render_camera_.WorldToScreen(marker->position);
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
                                   player_characters_.DeathClip());
    last_animation_sprite_.clear();
    last_animation_state_ = engine::animation::LocomotionState::Idle;
    animation_logs_remaining_ = 8;
    ApplyActiveCharacterStats();
    ConfigureAbilities();
    const auto& character = player_characters_.Active();
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
    data.map_seed = map_seed_;
    for (const auto& plot : plant_system_.Plots()) {
        if (!plot.species.empty()) data.plants.push_back(plot.species);
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
        if (data.map_seed != map_seed_) {
            map_seed_ = data.map_seed;
            ConfigureWorldGraph();
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
                         scene_.id.rfind("r1_", 0) == 0;
    current_chest_definition_ = "chest00";
    if (scene_.id.find("r1_2") != std::string::npos) {
        current_chest_definition_ = "chest01";
    } else if (scene_.id.find("r1_3") != std::string::npos) {
        current_chest_definition_ = "chest02";
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
               (cleared_rooms_.contains(scene_.id) ||
                enemies_.AliveCount() == 0)) {
        item_world_.AddChest(current_chest_definition_,
                             scene_.spawn + engine::Vec2{0.0f, 2.4f});
        chest_spawned_rooms_.insert(scene_.id);
    }
    // The reference transfer_gate_02 is the actual portal object. The native
    // room scenes do not serialize a return gate, so place the recovered asset
    // at a deterministic clear floor position and expose the normal E prompt.
    if (dungeon) {
        const bool has_portal = std::any_of(
            scene_.interaction_markers.begin(), scene_.interaction_markers.end(),
            [](const engine::assets::InteractionMarker& marker) {
                return marker.kind == "portal";
            });
        if (!has_portal) {
            engine::assets::InteractionMarker portal;
            portal.id = "return_portal";
            portal.kind = "portal";
            portal.label = "RETURN TO LIVING ROOM";
            portal.position = scene_.spawn + engine::Vec2{4.5f, 3.0f};
            portal.radius = 3.2f;
            scene_.interaction_markers.push_back(std::move(portal));
        }
    }
}

void Application::TryInteract() {
    const auto* marker = NearbyInteraction();
    if (marker == nullptr) {
        if (NearOpenDoor()) SwitchRoom();
        return;
    }
    if (marker->kind == "character") {
        game_ui_.OpenCharacterSelect(
            static_cast<int>(player_characters_.ActiveIndex()));
        LOG_INFO_APP("Living Room: character station opened");
    } else if (marker->kind == "forge") {
        game_ui_.OpenWeaponSelect(
            static_cast<int>(weapon_inventory_.ActiveIndex()));
        LOG_INFO_APP("Living Room: forge/weapon station opened (recipe system UNRECOVERED)");
    } else if (marker->kind == "garden") {
        const int plot = plant_system_.NearestPlot(player_.Position(), 4.0f);
        if (plot >= 0) {
            const auto result = plant_system_.Interact(plot, "carrot_dev");
            if (result == game::hub::PlantInteraction::Planted) {
                LOG_INFO_APP("Garden: planted seed in plot {} (5 s dev timer)",
                             plot);
            } else if (result == game::hub::PlantInteraction::Harvested) {
                LOG_INFO_APP("Garden: harvested plot {} (harvested={})",
                             plot, plant_system_.HarvestedCount());
            } else if (result == game::hub::PlantInteraction::NotReady) {
                LOG_INFO_APP("Garden: plot {} is still growing", plot);
            }
        } else {
            LOG_INFO_APP("Living Room: garden station (walk closer to a plot)");
        }
    } else if (marker->kind == "trader") {
        game_ui_.OpenTrader();
        LOG_INFO_APP("Trader: opened orb weapon inventory");
        PlayUiSound("ui_click");
    } else if (marker->kind == "portal") {
        const double now = NowSeconds();
        if (return_portal_confirm_seconds_ >= 0.0 &&
            now - return_portal_confirm_seconds_ <= 3.0) {
            CaptureCurrentRoomState();
            return_portal_confirm_seconds_ = -1.0;
            if (ActivateRoom(world_hub_id_, "")) {
                LOG_INFO_APP("Portal: returned to Living Room");
            }
        } else {
            return_portal_confirm_seconds_ = now;
            LOG_INFO_APP("Portal: E again within 3 s to confirm return");
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
    if (!secondary_weapon_.Configure(definition)) secondary_weapon_.Clear();
}

void Application::ConfigureTraderOffers() {
    trader_offers_.clear();
    if (weapons_.Count() == 0) return;
    int level = 1;
    engine::RGRandomStream random;
    random.SetRandomSeed(static_cast<int32_t>(
        (static_cast<std::uint32_t>(map_seed_) ^ 0x4F524253u) & 0x7FFFFFFFu));
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
    LOG_INFO_APP("Trader: {} orb offers generated", trader_offers_.size());
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
    cleared_rooms_.erase(scene_.id);
    chest_spawned_rooms_.erase(scene_.id);
    room_enemy_snapshots_.erase(scene_.id);
    item_world_.Reset();
    player_coins_ = 0;
    ConfigureRoomItems();
    obstacles_.Select(0);
    weapon_inventory_.Configure(weapons_.Count());
    ConfigureActiveWeapon();
    ConfigureSecondaryWeapon();
    ConfigureAbilities();
    weapon_pickups_.Clear();
    player_characters_.Select(0);
    ApplyActiveCharacterStats();
    ConfigureAbilities();
    room_clear_pending_ = false;
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
    locomotion_animator_.Configure(player_characters_.IdleClip(),
                                   player_characters_.RunClip(),
                                   player_characters_.DeathClip());
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

void Application::FixedUpdate(double dt) {
    simulation_time_ += dt;
    ++simulation_ticks_;

    if (!scene_mode_) return;
    hit_flash_timer_ = std::max(0.0f, hit_flash_timer_ - static_cast<float>(dt));
    room_title_timer_ = std::max(0.0f, room_title_timer_ - static_cast<float>(dt));
    room_transition_flash_ = std::max(0.0f,
        room_transition_flash_ - static_cast<float>(dt));
    player_energy_.Update(static_cast<float>(dt));
    asset_lab_.Update(static_cast<float>(dt));
    weapon_pickups_.Update(static_cast<float>(dt));
    plant_system_.Update(static_cast<float>(dt));

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
            attack_held_ ? weapon_.Definition().atk_move_speed : 0.0f);
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
        if (shots > 0) PlayEffect("weapon_fire", 0.62f);
        if (shots > 0 && weapon_.Definition().is_melee) {
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

    camera_.Follow(player_.Position(), static_cast<float>(dt));

    projectiles_.FixedUpdate(static_cast<float>(dt), collision_world_);
    if (config_.locomotion_debug &&
        (projectiles_.WorldHitsThisStep() > 0 ||
         projectiles_.LifetimeExpirationsThisStep() > 0)) {
        LOG_INFO_APP("Projectiles: world_hits={} lifetime_expired={} active={}",
                     projectiles_.WorldHitsThisStep(),
                     projectiles_.LifetimeExpirationsThisStep(),
                     projectiles_.ActiveCount());
    }
    enemies_.ResolveProjectileHits(projectiles_);
    bool player_damaged = enemies_.FixedUpdate(
        static_cast<float>(dt), player_.Position(), player_health_,
        &collision_world_, &projectiles_);
    player_damaged = projectiles_.ResolvePlayerHits(
        player_.Position(), player_health_) || player_damaged;
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
    UpdateAudio();
    UpdateRoomClear();
    if (player_damaged) {
        PlayEffect("player_hit");
        hit_flash_timer_ = 0.12f;
        LOG_INFO_APP("Player damaged: hp={} armor={} dead={}",
                     player_health_.hp, player_health_.armor,
                     BooleanText(player_health_.dead));
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
                     !asset_lab_.IsOpen()
        ? platform::ReadDigitalMovement(snapshot)
        : platform::MovementInput{};
    if (!scene_mode_) return;
    // NATIVE TEST BINDING: the reference uses a mobile attack button
    // (RGJoyControl.UpdateBtnAtkClick -> RGController.BtnAtkClick).
    // Keyboard Space/J and left mouse are the native dev equivalents.
    attack_held_ = !dev_ui_.MenuOpen() && !asset_lab_.IsOpen() &&
                   (snapshot.IsHeld(platform::Action::Attack) ||
                    snapshot.mouse_pressed[0]);
    UpdateAimInput(&snapshot);
}

bool Application::LoadWorldRooms() {
    world_templates_ = {
        {"r1_1_visual", "scenes/r1_1_visual/scene.json"},
        {"r1_2_visual", "scenes/r1_2_visual/scene.json"},
        {"r1_3_visual", "scenes/r1_3_visual/scene.json"},
    };
    multi_room_world_ = false;
    const auto known_scene = [&](const std::string& path) {
        if (path == world_hub_path_) return true;
        for (const auto& room : world_templates_) {
            if (room.scene_path == path) return true;
        }
        return false;
    };
    if (!known_scene(config_.scene_path)) {
        LOG_INFO_APP("Custom scene mode: multi-room world disabled");
        return false;
    }
    map_seed_ = config_.map_seed;
    const DungeonPlan plan = BuildDungeonPlan(map_seed_, world_templates_);

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
    if (world_templates_.empty()) return;
    const DungeonPlan plan = BuildDungeonPlan(map_seed_, world_templates_);
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
    const std::string level_prefix = "1";
    int next_code = 1;
    for (const std::string& id : queue) {
        if (id == world_hub_id_) continue;
        room_stage_labels_[id] = level_prefix + "-" +
            std::to_string(next_code++);
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
    if (enemies_.AliveCount() != 0) return;
    room_clear_pending_ = false;
    cleared_rooms_.insert(scene_.id);
    room_doors_.OpenAll();
    if (!scene_.is_hub && !chest_spawned_rooms_.contains(scene_.id)) {
        item_world_.AddChest(current_chest_definition_,
                             scene_.spawn + engine::Vec2{0.0f, 2.4f});
        chest_spawned_rooms_.insert(scene_.id);
        LOG_INFO_APP("Room clear: silver chest appeared ({})",
                     current_chest_definition_);
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
        (void)weapon_inventory_.EquipActive(0);
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
                                   player_characters_.DeathClip());
    if (player_dead_) locomotion_animator_.PlayDead();
    last_animation_sprite_.clear();
    last_animation_state_ = engine::animation::LocomotionState::Idle;
    animation_logs_remaining_ = 8;
    player_render_ = engine::rendering::BindPlayerRenderInstances(scene_);
    if (!player_render_.IsValid()) {
        LOG_ERROR_APP("Room switch player render binding missing");
        return false;
    }
    std::string enemy_error;
    if (!enemies_.Configure(scene_, &enemy_error)) {
        LOG_ERROR_APP("Room switch enemy setup failed: {}", enemy_error);
        return false;
    }
    RestoreCurrentRoomState();
    (void)obstacles_.Configure(scene_, &error);
    ConfigureRoomItems();
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
    projectiles_.Clear();
    weapon_pickups_.Clear();
    if (scene_.is_hub) {
        plant_system_.Clear();
        plant_system_.AddPlot("garden_1", {-27.0f, -2.5f}, 5.0f);
        plant_system_.AddPlot("garden_2", {-25.0f, -2.5f}, 5.0f);
        plant_system_.AddPlot("garden_3", {-27.0f, 0.0f}, 5.0f);
        plant_system_.AddPlot("garden_4", {-25.0f, 0.0f}, 5.0f);
    }
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
