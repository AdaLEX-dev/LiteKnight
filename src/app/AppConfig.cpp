// Soul Knight Linux — Application configuration implementation
#include "AppConfig.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <string_view>

namespace soulknight::app {

bool IsWaylandDriver(std::string_view driver) noexcept {
    return driver == "wayland";
}

AppConfig ParseCommandLine(int argc, char* argv[]) {
    AppConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i] != nullptr ? argv[i] : "";

        if (arg == "--help" || arg == "-h") {
            config.help_requested = true;
        } else if (arg == "--smoke-test") {
            config.smoke_test_seconds = 5.0;
        } else if (arg == "--exercise-window") {
            config.exercise_window = true;
        } else if (arg == "--locomotion-debug") {
            config.locomotion_debug = true;
        } else if (arg == "--devroom") {
            config.devroom = true;
            config.scene_path = "scenes/r1_1_visual/scene.json";
            config.title = "LiteKnight Devroom";
        } else if (arg == "--start-gui") {
            config.start_gui = true;
            config.scene_path = "scenes/hero_room/scene.json";
            config.title = "LiteKnight";
        } else if (arg == "--hub") {
            config.devroom = true;
            config.scene_path = "scenes/hero_room/scene.json";
            config.title = "LiteKnight Living Room";
        } else if (arg == "--fullscreen") {
            config.fullscreen = true;
        } else if (arg == "--gameplay-smoke" ||
                   arg == "--extended-gameplay") {
            config.gameplay_smoke = true;
            config.scene_path = "scenes/hero_room/scene.json";
            config.title = "LiteKnight Gameplay Smoke";
        } else if (arg == "--asset-lab") {
            config.asset_lab = true;
            config.scene_path = "scenes/r1_1_visual/scene.json";
            config.title = "LiteKnight Asset Lab";
        } else if (arg.rfind("--interactive-test=", 0) == 0) {
            config.interactive_test = true;
            config.interactive_test_name = std::string(arg.substr(19));
            config.scene_path = "scenes/r1_1_visual/scene.json";
            config.title = "LiteKnight Interactive Test";
            const std::string& test = config.interactive_test_name;
            if (test == "living_room" || test == "test-living-room" ||
                test == "character_select" || test == "test-character-select" ||
                test == "weapon_station") {
                config.scene_path = "scenes/hero_room/scene.json";
            }
            if (test == "home" || test == "test-home" ||
                test == "main_menu") {
                config.scene_path = "scenes/hero_room/scene.json";
                config.start_gui = true;
            }
        } else if (arg == "--interactive-test" && i + 1 < argc) {
            config.interactive_test = true;
            config.interactive_test_name = argv[i + 1];
            config.scene_path = "scenes/r1_1_visual/scene.json";
            config.title = "LiteKnight Interactive Test";
            ++i;
        } else if (arg == "--replay" && i + 1 < argc) {
            config.replay = true;
            config.replay_path = argv[i + 1];
            config.scene_path = "scenes/r1_1_visual/scene.json";
            config.title = "LiteKnight Replay";
            ++i;
        } else if (arg.rfind("--replay=", 0) == 0) {
            config.replay = true;
            config.replay_path = std::string(arg.substr(9));
            config.scene_path = "scenes/r1_1_visual/scene.json";
            config.title = "LiteKnight Replay";
        } else if (arg == "--texture" && i + 1 < argc) {
            config.texture_path = argv[i + 1];
            ++i;
        } else if (arg == "--scene" && i + 1 < argc) {
            config.scene_path = argv[i + 1];
            ++i;
        } else if ((arg == "--area" || arg == "--review-area") &&
                   i + 1 < argc) {
            // Direct developer launch area. This is not a timed review: the
            // application runs normally until the window is closed.
            config.review_mode = true;
            config.review_area = argv[i + 1];
            ++i;
        } else if (arg.rfind("--area=", 0) == 0) {
            config.review_mode = true;
            config.review_area = std::string(arg.substr(7));
        } else if (arg.rfind("--review-area=", 0) == 0) {
            config.review_mode = true;
            config.review_area = std::string(arg.substr(14));
        } else if (arg == "--map-seed" && i + 1 < argc) {
            errno = 0;
            char* end = nullptr;
            const long value = std::strtol(argv[i + 1], &end, 10);
            if (end != argv[i + 1] && *end == '\0' && errno == 0) {
                config.map_seed = static_cast<std::int32_t>(value);
                config.map_seed_set = true;
            }
            ++i;
        } else if (arg.rfind("--map-seed=", 0) == 0) {
            errno = 0;
            char* end = nullptr;
            const std::string value_text = std::string(arg.substr(11));
            const long value = std::strtol(value_text.c_str(), &end, 10);
            if (end != value_text.c_str() && *end == '\0' && errno == 0) {
                config.map_seed = static_cast<std::int32_t>(value);
                config.map_seed_set = true;
            }
        } else if (arg == "--smoke-seconds" && i + 1 < argc) {
            errno = 0;
            char* end = nullptr;
            const double value = std::strtod(argv[i + 1], &end);
            if (end != argv[i + 1] && *end == '\0' && errno == 0 && value >= 0.0) {
                config.smoke_test_seconds = value > 3600.0 ? 3600.0 : value;
            }
            ++i;
        }

        // Keep this a bootstrap parser, not a general option framework.
        // Unknown arguments are harmless for a desktop-launched executable.
    }

    if (config.review_mode) {
        const std::string& area = config.review_area;
        if (area == "main_menu" || area.empty()) {
            config.start_gui = true;
            config.scene_path = "scenes/hero_room/scene.json";
            config.title = "LiteKnight Review - Main Menu";
        } else if (area == "settings") {
            config.start_gui = true;
            config.scene_path = "scenes/hero_room/scene.json";
            config.title = "LiteKnight Review - Settings";
        } else if (area == "living_room") {
            config.devroom = true;
            config.scene_path = "scenes/hero_room/scene.json";
            config.title = "LiteKnight Review - Living Room";
        } else if (area == "workshop" || area == "garden" ||
                   area == "cellar" || area == "trader" ||
                   area == "trader-world" || area == "mech" ||
                   area == "reforging" || area == "planting" ||
                   area == "calibration" || area == "mech-ui" ||
                   area == "suggestions") {
            config.devroom = true;
            config.scene_path = "scenes/hero_room/scene.json";
            config.title = "LiteKnight Review - " + area;
        } else if (area == "devroom") {
            config.devroom = true;
            config.scene_path = "scenes/r1_1_visual/scene.json";
            config.title = "LiteKnight Review - Devroom";
        } else if (area == "chest") {
            config.devroom = true;
            config.scene_path = "scenes/r1_1_visual/scene.json";
            config.title = "LiteKnight Review - Chest Catalogue";
        } else if (area == "weapons") {
            config.devroom = true;
            config.scene_path = "scenes/hero_room/scene.json";
            config.title = "LiteKnight Review - Weapon Catalogue";
        } else if (area == "r_sell" || area == "r_statue" ||
                   area == "r_mineral" || area == "r_mercenary" ||
                   area == "r_mount" || area == "r_chest" ||
                   area == "r_weapon_provider" ||
                   area == "r_start_no_event" || area == "r_transgete") {
            config.devroom = true;
            config.scene_path = "scenes/" + area + "_visual/scene.json";
            config.title = "LiteKnight Review - " + area;
        } else if (area == "r1_1" || area == "r1_2" || area == "r1_3" ||
                   area == "dungeon-1-1" || area == "dungeon-1-2") {
            config.devroom = true;
            const std::string room = area == "dungeon-1-2" ? "r1_2" : "r1_1";
            config.scene_path = "scenes/" + room + "_visual/scene.json";
            config.title = "LiteKnight Review - " + area;
        } else if (area == "boss-room") {
            config.devroom = true;
            config.scene_path = "scenes/r1_boss1_visual/scene.json";
            config.title = "LiteKnight Review - boss-room";
        } else if (area == "pause") {
            config.start_gui = true;
            config.scene_path = "scenes/hero_room/scene.json";
            config.title = "LiteKnight Review - Pause";
        } else if (area == "ui_design") {
            config.start_gui = true;
            config.scene_path = "scenes/hero_room/scene.json";
            config.title = "LiteKnight Review - UI Design Mode";
        } else if (area == "fullscreen") {
            config.fullscreen = true;
            config.start_gui = true;
            config.scene_path = "scenes/hero_room/scene.json";
            config.title = "LiteKnight Review - Fullscreen";
        }
    }

    // Test/play executable default: start at the normal main menu. Explicit
    // --devroom, --scene, --asset-lab, interactive-test and smoke modes keep
    // their documented behavior.
    if (config.scene_path.empty() && !config.help_requested &&
        config.smoke_test_seconds <= 0.0 && !config.asset_lab &&
        !config.interactive_test && !config.replay) {
        config.start_gui = true;
        config.scene_path = "scenes/hero_room/scene.json";
        config.title = "LiteKnight";
    }
    return config;
}

} // namespace soulknight::app
