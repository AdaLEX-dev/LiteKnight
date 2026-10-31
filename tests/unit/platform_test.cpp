// Soul Knight Linux — platform/application contract tests
#include <doctest/doctest.h>

#include "app/AppConfig.hpp"
#include "app/Application.hpp"
#include "platform/wayland/WindowState.hpp"

using soulknight::app::AppConfig;
using soulknight::app::Application;
using soulknight::app::IsWaylandDriver;
using soulknight::app::ParseCommandLine;
using soulknight::platform::WindowState;

TEST_CASE("Application config defaults match the native target" * doctest::test_suite("unit")) {
    const AppConfig config;
    CHECK(config.window_width == 1920);
    CHECK(config.window_height == 1080);
    CHECK(config.wayland_only);
    CHECK(config.resizable);
    CHECK(config.high_dpi);
    CHECK(config.smoke_test_seconds == doctest::Approx(0.0));
    CHECK(config.texture_path == "test/phase4_smoke.png");
    CHECK(soulknight::app::kSimulationStepSeconds == doctest::Approx(0.02));
    CHECK(soulknight::app::kSimulationHz == doctest::Approx(50.0));
    CHECK(soulknight::app::kRenderTargetHz == doctest::Approx(60.0));
}

TEST_CASE("Wayland-only driver validation rejects non-Wayland backends" * doctest::test_suite("unit")) {
    CHECK(IsWaylandDriver("wayland"));
    CHECK_FALSE(IsWaylandDriver("x11"));
    CHECK_FALSE(IsWaylandDriver("xwayland"));
    CHECK_FALSE(IsWaylandDriver("kmsdrm"));
    CHECK_FALSE(IsWaylandDriver(""));
}

TEST_CASE("Bootstrap command line parses bounded smoke options" * doctest::test_suite("unit")) {
    char executable[] = "soul-knight";
    char smoke_seconds[] = "--smoke-seconds";
    char five[] = "5";
    char exercise[] = "--exercise-window";
    char texture_arg[] = "--texture";
    char texture_path[] = "test/f903.png";
    char unknown[] = "--not-an-option";
    char* argv[] = {executable, smoke_seconds, five, exercise, texture_arg, texture_path, unknown};

    const AppConfig config = ParseCommandLine(7, argv);
    CHECK(config.smoke_test_seconds == doctest::Approx(5.0));
    CHECK(config.exercise_window);
    CHECK(config.texture_path == "test/f903.png");
    CHECK_FALSE(config.help_requested);

    char smoke_test[] = "--smoke-test";
    char* argv_default[] = {executable, smoke_test};
    const AppConfig default_config = ParseCommandLine(2, argv_default);
    CHECK(default_config.smoke_test_seconds == doctest::Approx(5.0));
}

TEST_CASE("Window state transitions are deterministic and idempotent" * doctest::test_suite("unit")) {
    WindowState state;
    CHECK(state.focused);
    CHECK_FALSE(state.minimized);
    CHECK_FALSE(state.close_requested);

    state.OnLogicalResize(1600, 900);
    CHECK(state.logical_width == 1600);
    CHECK(state.logical_height == 900);
    CHECK(state.resize_pending);
    state.ClearResizeRequest();
    CHECK_FALSE(state.resize_pending);

    state.OnPixelResize(3200, 1800);
    CHECK(state.pixel_width == 3200);
    CHECK(state.pixel_height == 1800);
    CHECK(state.resize_pending);
    state.ClearResizeRequest();

    state.OnMinimized();
    CHECK(state.minimized);
    state.OnRestored();
    CHECK_FALSE(state.minimized);
    CHECK(state.resize_pending);
    state.ClearResizeRequest();

    state.OnFocusLost();
    CHECK_FALSE(state.focused);
    state.OnFocusGained();
    CHECK(state.focused);

    state.RequestClose();
    CHECK(state.close_requested);

    // Zero/negative geometry events must not corrupt the state.
    state.OnLogicalResize(0, -1);
    state.OnPixelResize(-5, 0);
    CHECK(state.logical_width == 1600);
    CHECK(state.logical_height == 900);
    CHECK(state.pixel_width == 3200);
    CHECK(state.pixel_height == 1800);
}

TEST_CASE("Application shutdown is safe before full initialization" * doctest::test_suite("unit")) {
    Application application;
    CHECK_FALSE(application.IsInitialized());
    application.Shutdown();
    application.Shutdown();
    CHECK_FALSE(application.IsInitialized());
    CHECK_FALSE(application.IsRunning());
}
