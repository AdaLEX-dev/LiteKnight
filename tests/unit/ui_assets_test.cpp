// Soul Knight Linux — local recovered UI atlas/font loading tests
#include <doctest/doctest.h>

#include "engine/rendering/BitmapFont.hpp"
#include "game/ui/UIAssets.hpp"
#include "platform/filesystem/FileSystem.hpp"

using soulknight::engine::rendering::BitmapFont;
using soulknight::game::ui::UIAssets;
using soulknight::platform::FileSystem;

TEST_CASE("Recovered UI atlas loads when local assets are prepared" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    if (!filesystem.Exists("ui/manifest.json")) {
        return; // CI metadata-only checkout: local UI pixels are absent.
    }
    UIAssets assets;
    std::string error;
    CHECK(assets.Load(filesystem, "ui/manifest.json", &error));
    CHECK(assets.IsLoaded());
    CHECK(assets.Find("title_en0") != nullptr);
    CHECK(assets.Find("ui_33") != nullptr);
    CHECK(assets.Find("ui_34") != nullptr);
    CHECK(assets.Find("ui_67") != nullptr);
    CHECK(assets.Find("ui_94") != nullptr);
    CHECK(assets.Find("ui_btn1") != nullptr);
    CHECK_FALSE(assets.Textures().empty());
}

TEST_CASE("Recovered pixel font metrics are readable when available" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    if (!filesystem.Exists("ui/font/pixel_bold.json")) {
        return;
    }
    BitmapFont font;
    std::string error;
    CHECK(font.Load(filesystem, "ui/font/pixel_bold.json", &error));
    CHECK(font.IsValid());
    CHECK(font.LineHeight() >= 20);
    CHECK(font.Ascent() > 0);
    CHECK(font.Find('A') != nullptr);
    CHECK(font.Find('0') != nullptr);
    CHECK(font.MeasureWidth("NEW GAME", 1.0f) > 0.0f);
}
