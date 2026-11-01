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
    // D-ROOM moved to the upper-right corner in 0.5.4.
    CHECK(ui.HandlePointerClick({1640.0f, 70.0f}));
    CHECK(ui.TakeAction() == UIAction::OpenDevroom);
    ui.OpenMainMenu();
    CHECK(ui.HandlePointerClick({1850.0f, 115.0f}));
    CHECK(ui.TakeAction() == UIAction::OpenDevroom);
    ui.OpenMainMenu();
    CHECK(ui.HandlePointerClick({720.0f, 430.0f}));
    CHECK(ui.TakeAction() == UIAction::OpenSaveLoad);
    ui.OpenSaveLoad();
    CHECK(ui.Screen() == UIScreen::SaveLoad);
    CHECK_FALSE(ui.HandlePointerClick({900.0f, 500.0f}));
    CHECK(ui.TakeAction() == UIAction::None);
}

TEST_CASE("Settings routes real aim, fullscreen and design actions" *
          doctest::test_suite("unit")) {
    GameUI ui;
    ui.OpenSettings();
    CHECK(ui.Screen() == UIScreen::Settings);
    ui.Confirm();
    CHECK(ui.TakeAction() == UIAction::ToggleAimMode);
    ui.Navigate(1);
    ui.Confirm();
    CHECK(ui.TakeAction() == UIAction::ToggleFullscreen);
    ui.Navigate(2);
    ui.Confirm();
    CHECK(ui.TakeAction() == UIAction::OpenUiDesignMode);
    ui.Navigate(2);
    CHECK(ui.Screen() == UIScreen::Settings);
}

TEST_CASE("Gameplay uses the compact HUD and has no fullscreen map state" *
          doctest::test_suite("unit")) {
    GameUI ui;
    ui.OpenGameplay();
    CHECK(ui.Screen() == UIScreen::Gameplay);
    CHECK_FALSE(ui.IsPaused());
}

TEST_CASE("Weapon selector navigation wraps across the integrated catalogue" *
          doctest::test_suite("unit")) {
    GameUI ui;
    ui.OpenGameplay();
    ui.SetWeaponCount(15);
    ui.OpenWeaponSelect(0);
    ui.Navigate(-1);
    CHECK(ui.WeaponSelection() == 14);
    ui.Navigate(1);
    CHECK(ui.WeaponSelection() == 0);
    ui.Navigate(1);
    CHECK(ui.WeaponSelection() == 1);
    ui.Confirm();
    CHECK(ui.TakeAction() == UIAction::SelectWeapon);
}

TEST_CASE("Orb trader opens, navigates and routes purchases" *
          doctest::test_suite("unit")) {
    GameUI ui;
    ui.OpenGameplay();
    ui.OpenTrader();
    CHECK(ui.Screen() == UIScreen::Trader);
    CHECK(ui.IsPaused());
    CHECK(ui.TraderSelection() == 0);
    ui.Navigate(1);
    CHECK(ui.TraderSelection() == 1);
    ui.Navigate(2);
    CHECK(ui.TraderSelection() == 0);
    ui.Confirm();
    CHECK(ui.TakeAction() == UIAction::BuyTraderItem);
    ui.Back();
    CHECK(ui.Screen() == UIScreen::Gameplay);
}
