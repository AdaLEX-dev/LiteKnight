// Soul Knight Linux — Application lifecycle implementation
#include "Application.hpp"

#include "engine/assets/PngDecoder.hpp"
#include "engine/assets/SceneLoader.hpp"
#include "engine/animation/AnimationClip.hpp"
#include "engine/core/Log.hpp"
#include "engine/rendering/VulkanRenderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <chrono>
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
    // NATIVE DESIGN: r1_1 is 15x15 with a 1-unit wall ring. If a scene omits
    // explicit native collision data, use the resulting 13x13 interior.
    constexpr float kInteriorHalfExtent = 6.5f;
    return engine::collision::Aabb{
        {center.x - kInteriorHalfExtent, center.y - kInteriorHalfExtent},
        {center.x + kInteriorHalfExtent, center.y + kInteriorHalfExtent}
    };
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
        const engine::animation::AnimationClip* idle_clip = nullptr;
        const engine::animation::AnimationClip* run_clip = nullptr;
        for (const engine::animation::AnimationClip& clip : scene_.animations) {
            if (clip.id == "c00_idle") idle_clip = &clip;
            if (clip.id == "c00_run") run_clip = &clip;
            LOG_INFO_APP("Animation: {} ({} s, {} Hz, loop={}, tracks={})",
                         clip.id, clip.duration, clip.sample_rate,
                         BooleanText(clip.loop), clip.tracks.size());
            for (const engine::animation::SpriteTrack& track : clip.tracks) {
                LOG_INFO_APP("Animation track: {} keys={}", track.target_path,
                             track.keys.size());
            }
        }
        locomotion_animator_.Configure(idle_clip, run_clip);
        player_ = engine::locomotion::PlayerLocomotion(scene_.spawn);
        if (scene_.collision_bounds.w > 0.0f && scene_.collision_bounds.h > 0.0f) {
            collision_world_.SetBounds(engine::collision::Aabb{
                {scene_.collision_bounds.x, scene_.collision_bounds.y},
                {scene_.collision_bounds.x + scene_.collision_bounds.w,
                 scene_.collision_bounds.y + scene_.collision_bounds.h}
            });
        } else {
            collision_world_.SetBounds(MakeFallbackCollisionBounds(camera_.position));
        }
        LOG_INFO_APP("Locomotion: spawn ({:.2f},{:.2f}), effective speed {:.2f}, idle={}, run={}",
                     scene_.spawn.x, scene_.spawn.y, player_.Config().EffectiveSpeed(),
                     idle_clip != nullptr ? "yes" : "no",
                     run_clip != nullptr ? "yes" : "no");
        LOG_INFO_APP("Collision: native room AABB [{:.2f},{:.2f}]..[{:.2f},{:.2f}]",
                     collision_world_.Bounds().min.x, collision_world_.Bounds().min.y,
                     collision_world_.Bounds().max.x, collision_world_.Bounds().max.y);
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

    for (const engine::assets::SpriteInstance& instance : scene_.instances) {
        if (instance.sprite_index >= scene_.sprites.size()) continue;
        const engine::assets::SpriteData* sprite =
            &scene_.sprites[instance.sprite_index];
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
        draw.position = instance.position;
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
        draw.flip_y = instance.flip_y;
        (void)sprite_batch_.DrawSprite(draw);
    }
    // BuildRenderBatch is called after fixed updates while an animation is
    // active; avoid spamming the log from the per-frame path.
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

void Application::FixedUpdate(double dt) {
    simulation_time_ += dt;
    ++simulation_ticks_;

    if (!scene_mode_) return;

    player_.FixedUpdate(movement_input_.direction, movement_input_.weight,
                        static_cast<float>(dt), collision_world_);
    locomotion_animator_.Update(player_.IsMoving(), static_cast<float>(dt));
    camera_.Follow(player_.Position(), static_cast<float>(dt));

    if (config_.locomotion_debug && simulation_ticks_ % 25 == 0) {
        const engine::animation::SpriteKeyframe* frame =
            locomotion_animator_.Evaluate("c00/img/body");
        const auto& collision = player_.LastCollision();
        LOG_INFO_APP(
            "locomotion: pos=({:.3f},{:.3f}) input=({:.2f},{:.2f}) "
            "vel=({:.3f},{:.3f}) state={} frame={} blocked=({},{}) camera=({:.3f},{:.3f})",
            player_.Position().x, player_.Position().y,
            movement_input_.direction.x, movement_input_.direction.y,
            player_.Velocity().x, player_.Velocity().y,
            engine::animation::LocomotionStateName(locomotion_animator_.State()),
            frame != nullptr ? frame->sprite_id : "none",
            BooleanText(collision.blocked_x), BooleanText(collision.blocked_y),
            camera_.position.x, camera_.position.y);
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
