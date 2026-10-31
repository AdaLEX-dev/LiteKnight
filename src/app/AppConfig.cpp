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
