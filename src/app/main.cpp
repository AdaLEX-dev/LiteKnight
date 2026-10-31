// Soul Knight Linux — Main Entry Point
// Native CachyOS/Wayland port of Soul Knight 2.6.8

#include "app/AppConfig.hpp"
#include "app/Application.hpp"
#include "engine/core/Log.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <iostream>

using namespace soulknight;

namespace {

void PrintUsage(const char* executable) {
    std::cout
        << "Soul Knight native runtime (Phase 4 foundation)\n"
        << "Usage: " << executable << " [options]\n"
        << "  --smoke-test             run the Wayland/Vulkan smoke test for 5 seconds\n"
        << "  --smoke-seconds <secs>   run for a bounded duration\n"
        << "  --exercise-window        resize/minimize/restore during the smoke test\n"
        << "  --locomotion-debug       log fixed-step movement/input/collision state\n"
        << "  O (in scene mode)        toggle recovered r1_1 doors open/closed\n"
        << "  --texture <asset-path>   PNG relative to the native asset root\n"
        << "  --scene <asset-path>     native scene JSON relative to the asset root\n"
        << "  --help, -h               show this help\n";
}

} // namespace

int main(int argc, char* argv[]) {
    Log::Initialize();

    app::AppConfig config = app::ParseCommandLine(argc, argv);
    if (config.help_requested) {
        PrintUsage(argv[0] != nullptr ? argv[0] : "soul-knight");
        Log::Shutdown();
        return EXIT_SUCCESS;
    }

#if defined(DEBUG) || defined(_DEBUG)
    // Debug builds request validation when the system provides the layer.
    // RelWithDebInfo/Release do not depend on validation-layer availability.
    config.enable_validation = true;
#endif

    SDL_SetMainReady();

    app::Application application(config);
    const int result = application.Run();

    Log::Shutdown();
    return result;
}
