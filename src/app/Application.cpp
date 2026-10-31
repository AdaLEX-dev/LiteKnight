// Soul Knight Linux — Application lifecycle implementation
#include "Application.hpp"

#include "engine/assets/PngDecoder.hpp"
#include "engine/assets/SceneLoader.hpp"
#include "engine/animation/AnimationClip.hpp"
#include "engine/core/Log.hpp"
#include "engine/rendering/VulkanRenderer.hpp"

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

    LOG_INFO_APP("Soul Knight native runtime");
    LOG_INFO_APP("Platform: {}", SDL_GetPlatform());
    LOG_INFO_APP("Version: {} ({})", SOUL_KNIGHT_VERSION, SOUL_KNIGHT_BUILD_TYPE);
    LOG_INFO_APP("Commit: {}", SOUL_KNIGHT_COMMIT_HASH);
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

    texture_uploads_.clear();
    camera_ = engine::Camera2D{};
    locomotion_animator_ = {};
    player_ = {};
    collision_world_ = {};
    player_render_ = {};
    room_doors_ = {};
    obstacles_ = {};
    player_characters_ = {};
    weapon_ = {};
    projectiles_.Clear();
    enemies_ = {};
    player_health_ = {};
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
        player_dead_ = false;
        aim_direction_ = {1.0f, 0.0f};
        projectiles_.Clear();
        std::string enemy_error;
        if (!enemies_.Configure(scene_, &enemy_error)) {
            LOG_FATAL_APP("Enemy configuration failed: {}", enemy_error);
            return false;
        }
        if (scene_.weapons.empty()) {
            LOG_ERROR_APP("No recovered weapon definition; attack disabled");
        } else {
            engine::assets::WeaponDefinition weapon_definition =
                scene_.weapons.front();
            if (scene_.player_stats.max_hp > 0) {
                weapon_definition.owner_deviation =
                    scene_.player_stats.deviation;
            }
            if (!weapon_.Configure(weapon_definition)) {
                LOG_ERROR_APP("Weapon configuration failed: {}",
                              weapon_definition.id);
            } else {
                LOG_INFO_APP("Weapon: {} projectile={} speed={:.1f} damage={} "
                             "deviation={} consume={} weapon_speed={:.2f}",
                             weapon_.Definition().id,
                             weapon_.Definition().projectile.id,
                             weapon_.Definition().bullet_speed,
                             weapon_.Definition().damage,
                             weapon_.Definition().deviation,
                             weapon_.Definition().consume,
                             weapon_.Definition().weapon_speed);
            }
        }
        LOG_INFO_APP("Combat: player hp={} armor={} camp={}; enemies={} "
                     "(definitions={}), projectiles=0",
                     player_health_.hp, player_health_.armor,
                     player_health_.camp, enemies_.Enemies().size(),
                     enemies_.Definitions().size());
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

        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.scancode == SDL_SCANCODE_ESCAPE && !event.key.repeat) {
            LOG_INFO_APP("Escape pressed; requesting shutdown");
            running_ = false;
        }

        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.scancode == SDL_SCANCODE_O && !event.key.repeat &&
            scene_mode_ && !scene_.doors.empty()) {
            ToggleAllDoors();
        }

        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.scancode == SDL_SCANCODE_C && !event.key.repeat &&
            scene_mode_ && !scene_.doors.empty()) {
            CloseAllDoors();
        }

        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            scene_mode_) {
            if (event.key.scancode == SDL_SCANCODE_LEFTBRACKET) {
                CycleCharacter(-1);
            } else if (event.key.scancode == SDL_SCANCODE_RIGHTBRACKET) {
                CycleCharacter(1);
            } else if (event.key.scancode == SDL_SCANCODE_P) {
                CycleObstacleVariant();
            } else if (event.key.scancode == SDL_SCANCODE_R) {
                ResetCombat();
            } else if (event.key.scancode >= SDL_SCANCODE_1 &&
                       event.key.scancode <= SDL_SCANCODE_9) {
                SwitchCharacter(static_cast<std::size_t>(
                    event.key.scancode - SDL_SCANCODE_1));
            } else if (event.key.scancode == SDL_SCANCODE_0) {
                SwitchCharacter(9);
            }
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

void Application::UpdateAimInput() {
    const platform::InputSnapshot& snapshot = input_manager_.GetSnapshot();
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
    player_dead_ = false;
    attack_held_ = false;
    weapon_.Release();
    aim_direction_ = {1.0f, 0.0f};
    mouse_aim_valid_ = false;
    facing_ = engine::locomotion::FacingDirection::Right;
    projectiles_.Clear();
    enemies_.Reset();
    room_doors_.OpenAll();
    RebuildCollisionWorld();
    camera_.position = scene_.spawn;
    locomotion_animator_.Configure(player_characters_.IdleClip(),
                                   player_characters_.RunClip());
    last_animation_sprite_.clear();
    last_animation_state_ = engine::animation::LocomotionState::Idle;
    animation_logs_remaining_ = 8;
    LOG_INFO_APP("Combat reset: spawn=({:.2f},{:.2f}) hp={} armor={} "
                 "enemies={} doors=open",
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
                                         projectiles_);
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
        static_cast<float>(dt), player_.Position(), player_health_);
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
        movement_input_ = scene_mode_
            ? platform::ReadDigitalMovement(input_manager_.GetSnapshot())
            : platform::MovementInput{};
        if (scene_mode_) {
            const platform::InputSnapshot& snapshot =
                input_manager_.GetSnapshot();
            // NATIVE TEST BINDING: the reference uses a mobile attack button
            // (RGJoyControl.UpdateBtnAtkClick -> RGController.BtnAtkClick).
            // Keyboard Space/J and left mouse are the native dev equivalents.
            attack_held_ = snapshot.IsHeld(platform::Action::Attack) ||
                           snapshot.mouse_pressed[0];
            UpdateAimInput();
        }
        if (!running_) break;
        if (SmokeTestExpired()) {
            LOG_INFO_APP("Smoke test duration reached");
            break;
        }

        frame_timer_.BeginFrame();

        while (frame_timer_.ShouldSimulate()) {
            FixedUpdate(frame_timer_.GetFixedDeltaTime());
            frame_timer_.ConsumeSimStep();
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

    running_ = false;
    Shutdown();
    return render_failed ? EXIT_FAILURE : EXIT_SUCCESS;
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
