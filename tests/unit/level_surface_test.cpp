// Soul Knight Linux — recovered level surface selection tests
#include <doctest/doctest.h>

#include "engine/assets/SceneAsset.hpp"
#include "game/room/LevelSurface.hpp"

using soulknight::engine::assets::LevelFloorVariant;
using soulknight::engine::assets::LevelSurfaceSprite;
using soulknight::engine::assets::LevelTheme;
using soulknight::engine::assets::LevelWallVariant;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::Vec2;
using soulknight::game::room::LevelSurfaceBuilder;

namespace {

LevelSurfaceSprite Sprite(int index, float offset_y = 0.0f) {
    LevelSurfaceSprite sprite;
    sprite.sprite_index = index;
    sprite.offset = {0.0f, offset_y};
    sprite.scale = {1.05f, 1.05f};
    sprite.sorting_layer = index < 100 ? 2 : 4;
    return sprite;
}

LevelTheme Theme(const std::string& level, const std::string& branch,
                 int floor_base, int wall_base) {
    LevelTheme theme;
    theme.level = level;
    theme.branch = branch;
    theme.manager = "map_" + branch + "1";
    for (int i = 0; i < 6; ++i) {
        LevelFloorVariant floor;
        floor.sprite = Sprite(floor_base + i);
        theme.floors.push_back(floor);
    }
    for (int i = 0; i < 2; ++i) {
        LevelWallVariant wall;
        wall.sprites.push_back(Sprite(wall_base + i * 10));
        wall.sprites.push_back(Sprite(wall_base + i * 10 + 1, 1.0f));
        theme.walls.push_back(wall);
    }
    return theme;
}

SceneAsset Room() {
    SceneAsset scene;
    scene.id = "r1_test";
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            scene.floor_cells.push_back(
                Vec2{static_cast<float>(x), static_cast<float>(y)});
        }
    }
    scene.wall_cells = {Vec2{0.0f, 3.0f}, Vec2{1.0f, 3.0f}};
    scene.corridor_wall_cells = {Vec2{2.0f, 3.0f}};
    scene.level_themes.push_back(Theme("1-1", "A", 100, 200));
    scene.level_themes.push_back(Theme("1-1", "B", 300, 400));
    scene.level_themes.push_back(Theme("2-1", "A", 500, 600));
    return scene;
}

} // namespace

TEST_CASE("Level surface selection finds branch A and level-specific themes" *
          doctest::test_suite("unit")) {
    SceneAsset scene = Room();
    const LevelTheme* first =
        LevelSurfaceBuilder::Find(scene, "1-1", "A");
    CHECK(first != nullptr);
    CHECK(first->manager == "map_A1");
    CHECK(first->floors.front().sprite.sprite_index == 100);
    const LevelTheme* second =
        LevelSurfaceBuilder::Find(scene, "2-1", "A");
    CHECK(second != nullptr);
    CHECK(second->floors.front().sprite.sprite_index == 500);
    const LevelTheme* branch_b =
        LevelSurfaceBuilder::Find(scene, "1-1", "B");
    CHECK(branch_b != nullptr);
    CHECK(branch_b->floors.front().sprite.sprite_index == 300);
}

TEST_CASE("Level surface generation is deterministic for a native room seed" *
          doctest::test_suite("unit")) {
    SceneAsset scene = Room();
    const LevelTheme* theme = LevelSurfaceBuilder::Find(scene, "1-1", "A");
    CHECK(theme != nullptr);
    const auto first = LevelSurfaceBuilder::Build(scene, *theme, 123456u);
    const auto second = LevelSurfaceBuilder::Build(scene, *theme, 123456u);
    CHECK(first.size() == second.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        CHECK(first[i].sprite_index == second[i].sprite_index);
        CHECK(first[i].position.x == second[i].position.x);
        CHECK(first[i].position.y == second[i].position.y);
    }
    // 3x3 floors plus 3 wall cells at two sprites per wall variant.
    CHECK(first.size() == 9 + 3 * 2);
}

TEST_CASE("Different campaign levels change surface sprites but not geometry" *
          doctest::test_suite("unit")) {
    SceneAsset scene = Room();
    const LevelTheme* first_theme =
        LevelSurfaceBuilder::Find(scene, "1-1", "A");
    const LevelTheme* second_theme =
        LevelSurfaceBuilder::Find(scene, "2-1", "A");
    CHECK(first_theme != nullptr);
    CHECK(second_theme != nullptr);
    const auto first =
        LevelSurfaceBuilder::Build(scene, *first_theme, 777u);
    const auto second =
        LevelSurfaceBuilder::Build(scene, *second_theme, 777u);
    CHECK(first.size() == second.size());
    CHECK(first.front().position.x == second.front().position.x);
    CHECK(first.front().position.y == second.front().position.y);
    CHECK(first.back().position.x == second.back().position.x);
    CHECK(first.back().position.y == second.back().position.y);
    bool changed = false;
    for (std::size_t i = 0; i < first.size(); ++i) {
        if (first[i].sprite_index != second[i].sprite_index) changed = true;
    }
    CHECK(changed);
}

TEST_CASE("Wall surfaces use only the recovered two wall variants" *
          doctest::test_suite("unit")) {
    SceneAsset scene = Room();
    const LevelTheme* theme = LevelSurfaceBuilder::Find(scene, "1-1", "A");
    CHECK(theme != nullptr);
    const auto surfaces = LevelSurfaceBuilder::Build(scene, *theme, 9u);
    for (std::size_t i = 9; i < surfaces.size(); ++i) {
        const int32_t sprite = surfaces[i].sprite_index;
        CHECK((sprite == 200 || sprite == 201 ||
               sprite == 210 || sprite == 211));
    }
}
