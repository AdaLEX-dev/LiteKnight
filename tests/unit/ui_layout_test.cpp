// Soul Knight Linux — persistent logical HUD layout tests
#include <doctest/doctest.h>

#include "game/ui/HudLayout.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

using soulknight::game::ui::HudLayout;

TEST_CASE("HUD layout defaults are logical 1920x1080 anchors" *
          doctest::test_suite("unit")) {
    HudLayout layout;
    CHECK(layout.RectFor("hp").x == doctest::Approx(16.0f));
    CHECK(layout.RectFor("hp").y == doctest::Approx(12.0f));
    CHECK(layout.RectFor("pause").x + layout.RectFor("pause").w <= 1920.0f);
    CHECK(layout.RectFor("ability").x > 100.0f);
    CHECK(layout.RectFor("ability").y > 900.0f);
    CHECK(layout.RectFor("minimap").w > 0.0f);
    CHECK(layout.RectFor("weapon_slots").y > 900.0f);
}

TEST_CASE("HUD layout edits clamp, persist and reset" *
          doctest::test_suite("unit")) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
        ("liteknight-ui-layout-" + std::to_string(stamp) + ".json");
    HudLayout layout;
    CHECK(layout.MoveTo("ability", {1700.0f, 900.0f}));
    CHECK(layout.RectFor("ability").x == doctest::Approx(1700.0f));
    CHECK(layout.MoveTo("ability", {5000.0f, 5000.0f}));
    CHECK(layout.RectFor("ability").x + layout.RectFor("ability").w <= 1920.0f);
    std::string error;
    CHECK(layout.Save(path, &error));
    CHECK(error.empty());
    HudLayout loaded;
    CHECK(loaded.Load(path, &error));
    CHECK(loaded.RectFor("ability").x == layout.RectFor("ability").x);
    loaded.ResetToDefaults();
    CHECK(loaded.RectFor("ability").x == 190.0f);
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("HUD layout rejects malformed files without losing defaults" *
          doctest::test_suite("unit")) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
        ("liteknight-ui-layout-bad-" + std::to_string(stamp) + ".json");
    {
        std::ofstream stream(path);
        stream << "not json";
    }
    HudLayout layout;
    std::string error;
    CHECK_FALSE(layout.Load(path, &error));
    CHECK_FALSE(error.empty());
    CHECK(layout.RectFor("hp").w > 0.0f);
    std::error_code ec;
    std::filesystem::remove(path, ec);
}
