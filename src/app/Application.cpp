// Soul Knight Linux — Application lifecycle implementation
#include "Application.hpp"
#include "AssetEnvironment.hpp"

#include "engine/assets/PngDecoder.hpp"
#include "engine/assets/SceneLoader.hpp"
#include "engine/animation/AnimationClip.hpp"
#include "engine/core/Log.hpp"
#include "engine/rendering/VulkanRenderer.hpp"
#include "game/weapons/WeaponRenderer.hpp"
#include "game/ui/AssetLab.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string_view>
#include <utility>

namespace soulknight::app {
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
    save_manager_ = std::make_unique<soulknight::game::save::SaveManager>(
        filesystem_.GetDataHome() / "liteknight" / "saves");
    LOG_INFO_APP("Native save slot: {}",
                 save_manager_->SlotPath().string());
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
    dev_ui_ = {};
    asset_lab_ = {};
    weapon_ = {};
    projectiles_.Clear();
    enemies_ = {};
    player_health_ = {};
    player_energy_ = {};
    aim_direction_ = {1.0f, 0.0f};
    mouse_aim_valid_ = false;
    facing_ = engine::locomotion::FacingDirection::Right;
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
        for (const engine::assets::TextureAsset& texture : scene_.textures) {
            texture_uploads_.push_back(engine::rendering::TextureUpload{
                texture.id, &texture.data, texture.filter, texture.wrap
            });
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
                                       player_characters_.RunClip());
        player_ = engine::locomotion::PlayerLocomotion(scene_.spawn);
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
        projectiles_.Clear();
        std::string enemy_error;
        if (!enemies_.Configure(scene_, &enemy_error)) {
            LOG_FATAL_APP("Enemy configuration failed: {}", enemy_error);
            return false;
        }
        if (!weapons_.Configure(scene_)) {
            LOG_ERROR_APP("No recovered weapon definition; attack disabled");
        } else {
            weapon_inventory_.Configure(weapons_.Count());
            ConfigureActiveWeapon();
            weapon_pickups_.Clear();
            if (weapons_.Count() > 2) {
                SpawnWeaponPickup(2);
            }
        }
        LOG_INFO_APP("Combat: player hp={} armor={} camp={}; enemies={} "
                     "(definitions={}), projectiles=0",
                     player_health_.hp, player_health_.armor,
                     player_health_.camp, enemies_.Enemies().size(),
                     enemies_.Definitions().size());
        if (config_.asset_lab) {
            asset_lab_.Open();
        }
        std::string door_error;
        if (!room_doors_.Configure(scene_, &door_error)) {
            LOG_FATAL_APP("Room door configuration failed: {}", door_error);
            return false;
        }
        door_trigger_inside_.assign(room_doors_.Count(), false);
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
    window_config.fullscreen = false;
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
    LOG_INFO_APP("Vulkan validation: {}",
                 BooleanText(renderer_->ValidationEnabled()));

    BuildRenderBatch();
    LOG_INFO_APP("Render batch: {} sprite instances",
                 sprite_batch_.GetDraws().size());

    warmup_frames_ = 30;
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

    for (std::size_t scene_index = 0; scene_index < scene_.instances.size();
         ++scene_index) {
        const engine::assets::SpriteInstance& instance =
            scene_.instances[scene_index];
        if (instance.sprite_index >= scene_.sprites.size()) continue;
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
        if (locomotion_animator_.IsPlaying() &&
            !(is_player_body && player_dead_)) {
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
            scene_, player_render_, instance_index, player_.Position());
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
        draw.layer = MapSortingLayer(instance.sorting_layer);
        draw.sorting_order = instance.sorting_order;
        draw.texture_id = sprite->texture_id;
        draw.pivot = sprite->pivot;
        draw.rotation = instance.rotation;
        draw.flip_x = instance.flip_x;
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
                draw.layer = MapSortingLayer(visual.sorting_layer);
                draw.sorting_order = visual.sorting_order;
                draw.texture_id = sprite.texture_id;
                draw.pivot = sprite.pivot;
                draw.rotation = visual.rotation;
                draw.flip_x = visual.flip_x;
                draw.flip_y = visual.flip_y;
                (void)sprite_batch_.DrawSprite(draw);
            }
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
            draw.layer = MapSortingLayer(definition.body_sorting_layer);
            draw.sorting_order = definition.body_sorting_order;
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
        draw.layer = MapSortingLayer(5);
        draw.sorting_order = 5;
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

    soulknight::game::weapons::WeaponRenderer::Draw(
        sprite_batch_, scene_, weapon_.Definition(), player_.Position(),
        aim_direction_, player_dead_);

    soulknight::game::ui::DevOverlayContext ui_context;
    ui_context.scene = &scene_;
    ui_context.camera = &camera_;
    ui_context.characters = &player_characters_;
    ui_context.weapons = &weapons_;
    ui_context.enemies = &enemies_;
    ui_context.player = &player_;
    ui_context.health = &player_health_;
    ui_context.energy = &player_energy_;
    ui_context.weapon_inventory = &weapon_inventory_;
    ui_context.room_id = scene_.id.c_str();
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
    dev_ui_.Draw(sprite_batch_, ui_context);
    asset_lab_.Draw(sprite_batch_, scene_, camera_, dev_ui_);
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
    if (interactive_test_.Active() && replay_ == nullptr) {
        interactive_test_.RecordCommand(
            simulation_ticks_ + 1, static_cast<int>(scancode),
            static_cast<int>(modifiers));
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
                } else if (scancode == SDL_SCANCODE_N) {
                    asset_lab_.StepFrame(1);
                } else if (scancode == SDL_SCANCODE_RETURN) {
                    ActivateAssetLabSelection();
                } else if (scancode == SDL_SCANCODE_F5) {
                    asset_lab_.Close();
                }
            } else if (scancode == SDL_SCANCODE_F2) {
                SaveGameToDisk();
            } else if (scancode == SDL_SCANCODE_F3) {
                LoadGameFromDisk();
            } else if (scancode == SDL_SCANCODE_F5) {
                asset_lab_.Open();
                LOG_INFO_APP("Asset laboratory opened");
            } else if (scancode == SDL_SCANCODE_F10) {
                dev_ui_.ToggleOverlay();
                LOG_INFO_APP("Developer overlay: {}",
                             BooleanText(dev_ui_.OverlayVisible()));
            } else if (scancode == SDL_SCANCODE_F9) {
                SpawnSelectedEnemy(1);
            } else if (scancode == SDL_SCANCODE_F11) {
                SpawnSelectedEnemy(5);
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
                TryPickupWeapon();
            } else if (scancode == SDL_SCANCODE_G) {
                SpawnWeaponPickup(weapons_.ActiveIndex());
            } else if (scancode == SDL_SCANCODE_O &&
                       !scene_.doors.empty()) {
                ToggleAllDoors();
            } else if (scancode == SDL_SCANCODE_C &&
                       !scene_.doors.empty()) {
                CloseAllDoors();
            } else if (scancode == SDL_SCANCODE_LEFTBRACKET) {
                CycleCharacter(-1);
            } else if (scancode == SDL_SCANCODE_RIGHTBRACKET) {
                CycleCharacter(1);
            } else if (scancode == SDL_SCANCODE_P) {
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
            } else if (scancode == SDL_SCANCODE_1) {
                SwitchWeaponSlot(0);
            } else if (scancode == SDL_SCANCODE_2) {
                SwitchWeaponSlot(1);
            }
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
            } else {
                LOG_INFO_APP("Escape pressed; requesting shutdown");
                running_ = false;
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
                                   player_characters_.RunClip());
    last_animation_sprite_.clear();
    last_animation_state_ = engine::animation::LocomotionState::Idle;
    animation_logs_remaining_ = 8;
    const auto& character = player_characters_.Active();
    LOG_INFO_APP("Character switch: [{}] {} (controller {}, body={}, idle={}, run={})",
                 player_characters_.ActiveIndex() + 1, character.id,
                 character.controller_name, character.body_sprite_id,
                 character.idle_animation_id, character.run_animation_id);
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
    const platform::InputSnapshot& snapshot =
        override != nullptr ? *override : input_manager_.GetSnapshot();
    // Reference path: RGController.AutoLock consumes the right-stick / locked
    // target direction into fixed_angle; Gun001.Attack uses that angle.
    // NATIVE TEST BINDINGS: right stick or mouse cursor (after it has moved)
    // provide the aim vector. Facing remains movement-driven and is a separate
    // concept, so strafing and aiming in different directions is supported.
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
            const int window_width = window_.GetWidth();
            const int window_height = window_.GetHeight();
            engine::Vec2 mouse = snapshot.mouse_position;
            if (window_width > 0 && window_height > 0) {
                mouse.x *= camera_.viewport_width /
                           static_cast<float>(window_width);
                mouse.y *= camera_.viewport_height /
                           static_cast<float>(window_height);
            }
            const engine::Vec2 world = camera_.ScreenToWorld(mouse);
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
    data.dead = player_health_.dead;
    data.playtime_seconds = static_cast<std::int64_t>(simulation_time_);
    std::string error;
    if (save_manager_->Save(data, &error)) {
        LOG_INFO_APP("Saved native game state: {}",
                     save_manager_->SlotPath().string());
    } else {
        LOG_ERROR_APP("Save failed: {}", error);
    }
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
    LOG_INFO_APP("Loaded native game state: {} character={} slot={} ",
                 save_manager_->SlotPath().string(), data.character_id,
                 data.active_slot + 1);
}

void Application::SpawnWeaponPickup(std::size_t weapon_index) {
    if (weapon_index >= weapons_.Count()) return;
    const engine::Vec2 position{
        player_.Position().x + 1.4f,
        player_.Position().y - 0.1f
    };
    if (weapon_pickups_.Spawn(weapon_index, position)) {
        LOG_INFO_APP("Weapon pickup spawned: {} at ({:.2f},{:.2f}) [E to take]",
                     weapons_.Definitions()[weapon_index].id, position.x,
                     position.y);
    }
}

void Application::TryPickupWeapon() {
    std::size_t weapon_index = 0;
    if (!weapon_pickups_.TakeNearest(player_.Position(), 1.5f,
                                     &weapon_index)) {
        LOG_INFO_APP("No weapon pickup in range (1.5)");
        return;
    }
    if (!weapon_inventory_.EquipActive(weapon_index)) return;
    ConfigureActiveWeapon();
    LOG_INFO_APP("Weapon pickup equipped into slot {}: {}",
                 weapon_inventory_.ActiveSlot() + 1,
                 weapons_.Definitions()[weapon_index].id);
}

void Application::SwitchWeaponSlot(std::size_t slot) {
    if (!weapon_inventory_.SwitchSlot(slot)) return;
    ConfigureActiveWeapon();
    LOG_INFO_APP("Weapon slot: {} primary={} secondary={} active={}",
                 weapon_inventory_.ActiveSlot() + 1,
                 weapon_inventory_.Slot(0), weapon_inventory_.Slot(1),
                 weapons_.Count() > 0 ? weapons_.Active().id : "none");
}

void Application::ConfigureActiveWeapon() {
    if (weapons_.Count() == 0) return;
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
    mouse_aim_valid_ = false;
    facing_ = engine::locomotion::FacingDirection::Right;
    projectiles_.Clear();
    enemies_.Reset();
    obstacles_.Select(0);
    weapon_inventory_.Configure(weapons_.Count());
    ConfigureActiveWeapon();
    weapon_pickups_.Clear();
    if (weapons_.Count() > 2) SpawnWeaponPickup(2);
    player_characters_.Select(0);
    room_doors_.OpenAll();
    door_trigger_inside_.assign(room_doors_.Count(), false);
    RebuildCollisionWorld();
    camera_.position = scene_.spawn;
    locomotion_animator_.Configure(player_characters_.IdleClip(),
                                   player_characters_.RunClip());
    last_animation_sprite_.clear();
    last_animation_state_ = engine::animation::LocomotionState::Idle;
    animation_logs_remaining_ = 8;
    LOG_INFO_APP("Developer reset: spawn=({:.2f},{:.2f}) hp={} armor={} "
                 "enemies={} doors=open character=c00 weapon=weapon_000 "
                 "obstacle=wall603",
                 scene_.spawn.x, scene_.spawn.y, player_health_.hp,
                 player_health_.armor, enemies_.Enemies().size());
}

void Application::UpdateDoorTriggers() {
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
    asset_lab_.Update(static_cast<float>(dt));
    weapon_pickups_.Update(static_cast<float>(dt));

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

    if (!player_dead_) {
        player_.SetSpeedRateBonus(
            attack_held_ ? weapon_.Definition().atk_move_speed : 0.0f);
        player_.FixedUpdate(movement_input_.direction, movement_input_.weight,
                            static_cast<float>(dt), collision_world_);
        facing_ = engine::locomotion::ResolveFacing(
            facing_, movement_input_.direction);
        locomotion_animator_.Update(player_.IsMoving(), static_cast<float>(dt));
        const int shots = weapon_.Update(static_cast<float>(dt), attack_held_,
                                         player_.Position(), aim_direction_,
                                         projectiles_, &player_energy_);
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
    const bool player_damaged = enemies_.FixedUpdate(
        static_cast<float>(dt), player_.Position(), player_health_,
        &collision_world_);
    if (player_damaged) {
        LOG_INFO_APP("Player damaged: hp={} armor={} dead={}",
                     player_health_.hp, player_health_.armor,
                     BooleanText(player_health_.dead));
    }
    if (player_health_.dead && !player_dead_) {
        player_dead_ = true;
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
    if (name == "two_weapon") {
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
    if (name == "save_load") {
        return "Move, switch weapons, spend energy, then F2 save and F3 load. "
               "Verify character/weapons/HP/energy/position restore. Enter "
               "finalizes.";
    }
    if (name == "room_transition") {
        return "Native room transitions are not recovered yet. O/C only open "
               "and close recovered r1_1 doors. Record this limitation and "
               "press Enter; do not treat door motion as a room transition.";
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
        TryPickupWeapon();
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
    state.weapon = weapons_.Count() > 0 ? weapon_.Definition().id : "none";
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

        while (frame_timer_.ShouldSimulate()) {
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
            // Animation time is advanced by fixed ticks, but the rendered
            // sprite is evaluated here so a frame always reflects the latest
            // deterministic animation state.
            BuildRenderBatch();
        }

        const auto& stats = frame_timer_.GetStats();
        if (window_.IsMinimized()) {
            // Do not busy-loop or submit frames for a zero-sized/minimized
            // surface. The compositor will send RESTORED when it is ready.
            SDL_Delay(16);
        } else {
            if (!renderer_->Render(sprite_batch_, camera_)) {
                LOG_FATAL_APP("Renderer frame failed");
                render_failed = true;
                running_ = false;
            } else {
                ++render_frames_;
                window_.ClearResizeRequest();
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
            LOG_INFO_PERF("frame_ms={:.3f} sim_steps={} render_frames={}",
                          stats.frame_time_ms, stats.sim_steps, render_frames_);
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
