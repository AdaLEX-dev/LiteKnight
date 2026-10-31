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
        << "LiteKnight native development executable\n"
        << "Usage: " << executable << " [options]\n"
        << "  --devroom                launch the playable LiteKnight devroom\n"
        << "  --asset-lab              open the recovered asset laboratory\n"
        << "  --start-gui              start at the native main menu (Living Room)\n"
        << "  --hub                    launch the recovered Living Room directly\n"
        << "  --fullscreen             start fullscreen\n"
        << "  --interactive-test=<id>  run/record a named serialized test\n"
        << "  --replay <result.json>   replay a serialized input stream\n"
        << "  --smoke-test             run the Wayland/Vulkan smoke test for 5 seconds\n"
        << "  --smoke-seconds <secs>   run for a bounded duration\n"
        << "  --map-seed <int>         deterministic native room-graph assignment\n"
        << "  --exercise-window        resize/minimize/restore during the smoke test\n"
        << "  --locomotion-debug       log fixed-step movement/input/collision state\n"
        << "  O (in scene mode)        toggle recovered r1_1 doors open/closed\n"
        << "  C (in scene mode)        close recovered r1_1 doors\n"
        << "  1..9,0 (in scene mode)   switch recovered test character\n"
        << "  [ / ] (in scene mode)    cycle recovered test characters\n"
        << "  P (in scene mode)        cycle recovered obstacle variant\n"
        << "  J / Space / LMB (in scene mode)\n"
        << "                           native test binding for the recovered attack\n"
        << "  Mouse / right stick / keypad 8,2,4,6\n"
        << "                           full 360-degree native test aim vector\n"
        << "  Q                        Knight c00 Dual Wield character power\n"
        << "  Tab / 1 / 2              next / primary / secondary weapon slot\n"
        << "  Esc                      pause/resume, settings back, close UI\n"
        << "  M                        open/close the map overlay\n"
        << "  E                        interact / open chest / pick up\n"
        << "  F6 / F7 / F8             character / weapon / enemy selector\n"
        << "  F9 / Shift+F9 / F12      spawn enemy / spawn x5 / clear enemies\n"
        << "  F11                      toggle bordered fullscreen\n"
        << "  F10                      toggle developer overlay\n"
        << "  R (in scene mode)        reset combat test state\n"
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
