// Soul Knight Linux — pause/settings hierarchy tests
#include <doctest/doctest.h>

#include "game/ui/GameUI.hpp"

using soulknight::game::ui::GameUI;
using soulknight::game::ui::UIAction;
using soulknight::game::ui::UIScreen;

TEST_CASE("Pause/settings hierarchy closes one level at a time" *
          doctest::test_suite("unit")) {
    GameUI ui;
    CHECK(ui.Screen() == UIScreen::Gameplay);
    ui.OpenPause();
    CHECK(ui.Screen() == UIScreen::Pause);
    CHECK(ui.IsPaused());
    ui.Navigate(1);
    ui.Confirm();
    CHECK(ui.TakeAction() == UIAction::Settings);
    ui.OpenSettings();
    CHECK(ui.Screen() == UIScreen::Settings);
    ui.Confirm();
    CHECK(ui.TakeAction() == UIAction::ToggleAimMode);
    ui.Back();
    CHECK(ui.Screen() == UIScreen::Pause);
    ui.Back();
    CHECK(ui.Screen() == UIScreen::Gameplay);
    CHECK_FALSE(ui.IsPaused());
}

TEST_CASE("Pause default selection resumes without toggling aim mode" *
          doctest::test_suite("unit")) {
    GameUI ui;
    ui.OpenPause();
    ui.Confirm();
    CHECK(ui.TakeAction() == UIAction::Resume);
    ui.Resume();
    CHECK(ui.Screen() == UIScreen::Gameplay);
}

TEST_CASE("Main-menu D-ROOM pointer target is clickable" *
          doctest::test_suite("unit")) {
    GameUI ui;
    ui.OpenMainMenu();
    CHECK(ui.HandlePointerClick({70.0f, 360.0f}));
    CHECK(ui.TakeAction() == UIAction::OpenDevroom);
    ui.OpenMainMenu();
    CHECK_FALSE(ui.HandlePointerClick({150.0f, 420.0f}));
    CHECK(ui.TakeAction() == UIAction::None);
}

TEST_CASE("Map overlay freezes gameplay and closes back to play" *
          doctest::test_suite("unit")) {
    GameUI ui;
    ui.OpenGameplay();
    ui.OpenMap();
    CHECK(ui.IsMapOpen());
    CHECK(ui.IsPaused());
    ui.CloseMap();
    CHECK(ui.Screen() == UIScreen::Gameplay);
    CHECK_FALSE(ui.IsPaused());
}
