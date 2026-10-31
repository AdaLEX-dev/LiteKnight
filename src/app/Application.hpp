// Soul Knight Linux — Application lifecycle
#pragma once

#include "AppConfig.hpp"
#include "platform/wayland/Window.hpp"
#include "platform/timing/FrameTimer.hpp"
#include "engine/rendering/SpriteBatch.hpp"

#include <memory>

namespace soulknight::engine::rendering {
class VulkanRenderer;
}

namespace soulknight::app {

// Owns the platform/runtime lifecycle. Gameplay must not call SDL or Vulkan
// directly; this is the only object that sequences platform and renderer
// shutdown.
class Application {
public:
    explicit Application(AppConfig config = {});
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Initialize();
    int Run();
    void RequestShutdown();
    void Shutdown();

    bool IsInitialized() const { return initialized_; }
    bool IsRunning() const { return running_; }
    const AppConfig& GetConfig() const { return config_; }

private:
    void PollEvents();
    void FixedUpdate(double dt);
    void ExerciseWindowIfRequested();
    void BuildSmokeScene();
    bool SmokeTestExpired() const;

    AppConfig config_;
    platform::Window window_;
    std::unique_ptr<engine::rendering::VulkanRenderer> renderer_;
    engine::rendering::SpriteBatch sprite_batch_;
    platform::FrameTimer frame_timer_;

    bool sdl_initialized_ = false;
    bool vulkan_library_loaded_ = false;
    bool initialized_ = false;
    bool running_ = false;
    bool shutdown_complete_ = false;

    double smoke_start_seconds_ = -1.0;
    double simulation_time_ = 0.0;
    unsigned long long simulation_ticks_ = 0;
    unsigned long long render_frames_ = 0;

    // Performance sample accumulation, deliberately measured rather than
    // asserted. Warm-up frames include initialization and are excluded.
    int warmup_frames_ = 0;
    int perf_samples_ = 0;
    double perf_total_frame_ms_ = 0.0;
    unsigned long long perf_total_sim_steps_ = 0;

    // Optional window-contract exercise for the manual smoke test.
    int exercise_stage_ = 0;
};

} // namespace soulknight::app
