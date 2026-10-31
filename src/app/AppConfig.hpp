// Soul Knight Linux — Application configuration
#pragma once

#include <string>
#include <string_view>

namespace soulknight::app {

// PROJECT DECISION: the native front-end defaults. The reference build target is
// 1920x1080 at a 60 FPS render cadence with a 50 Hz fixed simulation step.
// The 50 Hz value is REFERENCE-CONFIRMED in analysis/re/timing.md.
struct AppConfig {
    int window_width = 1920;
    int window_height = 1080;
    bool wayland_only = true;
    bool resizable = true;
    bool high_dpi = true;
    bool enable_validation = false;
    double smoke_test_seconds = 0.0;
    bool exercise_window = false;
    bool locomotion_debug = false;
    bool help_requested = false;
    std::string texture_path = "test/phase4_smoke.png";
    std::string scene_path;
    std::string title = "Soul Knight";
};

inline constexpr double kSimulationStepSeconds = 1.0 / 50.0;
inline constexpr double kSimulationHz = 50.0;
inline constexpr double kRenderTargetHz = 60.0;

// The platform contract is intentionally strict: only the native Wayland SDL
// driver is accepted. X11, XWayland, KMSDRM, and every other driver are invalid.
bool IsWaylandDriver(std::string_view driver) noexcept;

// Parse the small Phase 4 bootstrap CLI. Unknown arguments are ignored so the
// executable remains usable from a desktop launcher.
AppConfig ParseCommandLine(int argc, char* argv[]);

} // namespace soulknight::app
