// Soul Knight Linux — Input Tests
#include <doctest/doctest.h>
#include "platform/input/InputManager.hpp"
#include "platform/input/Action.hpp"
#include "platform/input/InputSnapshot.hpp"

using namespace soulknight::platform;

TEST_CASE("Input snapshot captures press/release/hold" * doctest::test_suite("unit")) {
    InputManager mgr;
    mgr.BindKey(SDL_SCANCODE_W, Action::MoveUp);

    mgr.InjectKeyDown(SDL_SCANCODE_W);
    mgr.ProcessEvents();
    CHECK(mgr.GetSnapshot().IsPressed(Action::MoveUp));
    CHECK(mgr.GetSnapshot().IsHeld(Action::MoveUp));

    mgr.ProcessEvents();
    CHECK(!mgr.GetSnapshot().IsPressed(Action::MoveUp));
    CHECK(mgr.GetSnapshot().IsHeld(Action::MoveUp));

    mgr.InjectKeyUp(SDL_SCANCODE_W);
    mgr.ProcessEvents();
    CHECK(mgr.GetSnapshot().IsReleased(Action::MoveUp));
    CHECK(!mgr.GetSnapshot().IsHeld(Action::MoveUp));
}

TEST_CASE("Analog deadzone applied" * doctest::test_suite("unit")) {
    InputManager mgr;
    mgr.InjectGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX, 0.1f);
    mgr.ProcessEvents();
    CHECK(mgr.GetSnapshot().move_x == 0.0f);

    mgr.InjectGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX, 0.5f);
    mgr.ProcessEvents();
    CHECK(mgr.GetSnapshot().move_x > 0.0f);
}

TEST_CASE("Input action mapping" * doctest::test_suite("unit")) {
    InputManager mgr;
    mgr.BindKey(SDL_SCANCODE_SPACE, Action::Attack);
    mgr.BindKey(SDL_SCANCODE_J, Action::Attack);

    mgr.InjectKeyDown(SDL_SCANCODE_SPACE);
    mgr.ProcessEvents();
    CHECK(mgr.GetSnapshot().IsPressed(Action::Attack));

    mgr.InjectKeyUp(SDL_SCANCODE_SPACE);
    mgr.InjectKeyDown(SDL_SCANCODE_J);
    mgr.ProcessEvents();
    CHECK(mgr.GetSnapshot().IsPressed(Action::Attack));
}
TEST_CASE("InputManager frame/event API matches the application event loop" *
          doctest::test_suite("unit")) {
    InputManager mgr;
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_D;
    event.key.down = true;

    mgr.BeginFrame();
    mgr.ProcessEvent(event);
    mgr.EndFrame();
    CHECK(mgr.GetSnapshot().IsPressed(Action::MoveRight));
    CHECK(mgr.GetSnapshot().IsHeld(Action::MoveRight));

    mgr.BeginFrame();
    mgr.EndFrame();
    CHECK_FALSE(mgr.GetSnapshot().IsPressed(Action::MoveRight));
    CHECK(mgr.GetSnapshot().IsHeld(Action::MoveRight));

    event.type = SDL_EVENT_KEY_UP;
    event.key.down = false;
    mgr.BeginFrame();
    mgr.ProcessEvent(event);
    mgr.EndFrame();
    CHECK(mgr.GetSnapshot().IsReleased(Action::MoveRight));
    CHECK_FALSE(mgr.GetSnapshot().IsHeld(Action::MoveRight));
}
