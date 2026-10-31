// Soul Knight Linux — Application configuration implementation
#include "AppConfig.hpp"

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
        } else if (arg == "--asset-lab") {
            config.asset_lab = true;
            config.scene_path = "scenes/r1_1_visual/scene.json";
            config.title = "LiteKnight Asset Lab";
        } else if (arg.rfind("--interactive-test=", 0) == 0) {
            config.interactive_test = true;
            config.interactive_test_name = std::string(arg.substr(19));
            config.scene_path = "scenes/r1_1_visual/scene.json";
            config.title = "LiteKnight Interactive Test";
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

    return config;
}

} // namespace soulknight::app
