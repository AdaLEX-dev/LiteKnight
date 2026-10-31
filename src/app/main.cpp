// Soul Knight Linux — Main Entry Point
// Native CachyOS/Wayland port of Soul Knight 2.6.8

#include "platform/wayland/Window.hpp"
#include "platform/input/InputManager.hpp"
#include "platform/timing/FrameTimer.hpp"
#include "platform/filesystem/FileSystem.hpp"
#include "platform/audio/AudioEngine.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/Types.hpp"
#include "platform/input/Action.hpp"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <memory>
#include <cstdlib>

using namespace soulknight;

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Initialize logging
    Log::Initialize();
    LOG_INFO("[APP]", "Soul Knight Linux starting...");
    LOG_INFO("[APP]", "Version: {}", SOUL_KNIGHT_VERSION);
    LOG_INFO("[APP]", "Build:   {}", SOUL_KNIGHT_BUILD_TYPE);
    LOG_INFO("[APP]", "Commit:  {}", SOUL_KNIGHT_COMMIT_HASH);

    // Initialize SDL (video + gamepad only, no audio)
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        LOG_FATAL("[APP]", "SDL_Init failed: {}", SDL_GetError());
        return EXIT_FAILURE;
    }
    LOG_INFO("[APP]", "SDL initialized");

    // Create window
    platform::WindowConfig window_config;
    window_config.title = "Soul Knight";
    window_config.width = 1920;
    window_config.height = 1080;
    window_config.vsync = true;
    window_config.fullscreen = false;

    auto window = std::make_unique<platform::Window>(window_config);
    if (!window->Initialize()) {
        LOG_FATAL("[APP]", "Window initialization failed");
        return EXIT_FAILURE;
    }
    LOG_INFO("[APP]", "Window created: {}x{}", window_config.width, window_config.height);

    // Initialize input
    platform::InputManager input_manager;
    input_manager.Initialize(window->GetSDLWindow());
    LOG_INFO("[APP]", "Input initialized");

    // Initialize filesystem
    platform::FileSystem filesystem;
    if (!filesystem.Initialize()) {
        LOG_FATAL("[APP]", "Filesystem initialization failed");
        return EXIT_FAILURE;
    }
    LOG_INFO("[APP]", "Filesystem initialized: data={}, config={}, cache={}",
             filesystem.GetDataHome().string(),
             filesystem.GetConfigHome().string(),
             filesystem.GetCacheHome().string());

    // Initialize audio
    platform::AudioEngine audio_engine;
    platform::AudioEngine::Config audio_config;
    if (!audio_engine.Initialize(audio_config)) {
        LOG_WARN("[APP]", "Audio initialization failed (continuing without audio)");
    } else {
        LOG_INFO("[APP]", "Audio initialized");
    }

    // Initialize frame timer
    platform::FrameTimer frame_timer;
    LOG_INFO("[APP]", "Frame timer initialized (target: 60 Hz)");

    // Main loop
    LOG_INFO("[APP]", "Entering main loop...");
    bool running = true;
    while (running) {
        // Begin frame timing
        frame_timer.BeginFrame();

        // Process input
        input_manager.ProcessEvents();
        platform::InputSnapshot input = input_manager.GetSnapshot();
        if (input.window_resized) {
            window->Resize(static_cast<int>(input.window_size.x), static_cast<int>(input.window_size.y));
        }
        if (!input.window_focused) {
            // Pause when unfocused
        }

        // Check for quit
        if (input.IsPressed(platform::Action::Pause)) {
            running = false;
        }

        // Fixed simulation step
        while (frame_timer.ShouldSimulate()) {
            const double dt = frame_timer.GetFixedDeltaTime();

            // TODO: Game simulation
            // game_state.Update(dt);
            // physics_system.Step(dt);
            // ai_system.Update(dt);
        }

        // Render (interpolated)
        const double alpha = frame_timer.GetInterpolationAlpha();
        // TODO: Render
        // renderer.Render(alpha);

        // End frame
        frame_timer.EndFrame();
        window->Present();
    }

    // Cleanup
    LOG_INFO("[APP]", "Shutting down...");
    audio_engine.Shutdown();
    window.reset();
    SDL_Quit();
    Log::Shutdown();

    LOG_INFO("[APP]", "Goodbye!");
    return EXIT_SUCCESS;
}