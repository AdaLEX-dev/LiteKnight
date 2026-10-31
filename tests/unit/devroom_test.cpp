// Soul Knight Linux — developer UI/registry/spawner primitives
#include <doctest/doctest.h>

#include "engine/assets/SceneAsset.hpp"
#include "engine/rendering/DebugText.hpp"
#include "game/enemy/EnemySystem.hpp"
#include "game/weapons/WeaponRegistry.hpp"

using soulknight::engine::assets::EnemyDefinition;
using soulknight::engine::assets::EnemyFixture;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::assets::WeaponDefinition;
using soulknight::engine::rendering::DebugTextRenderer;
using soulknight::game::enemy::EnemySystem;
using soulknight::game::weapons::WeaponRegistry;

TEST_CASE("Developer font atlas is generated without external assets" *
          doctest::test_suite("unit")) {
    DebugTextRenderer text;
    CHECK(text.BuildAtlas());
    CHECK(text.Atlas().width == 128);
    CHECK(text.Atlas().height == 48);
    CHECK(text.Atlas().rgba8.size() == 128U * 48U * 4U);
    CHECK(text.TextWidth("ABC", 2.0f) == doctest::Approx(36.0f));
    CHECK(text.LineHeight(2.0f) == doctest::Approx(20.0f));
}

TEST_CASE("Weapon registry cycles recovered definitions without mutating them" *
          doctest::test_suite("unit")) {
    SceneAsset scene;
    WeaponDefinition first;
    first.id = "weapon_000";
    first.damage = 3;
    WeaponDefinition second;
    second.id = "weapon_002";
    second.damage = 3;
    scene.weapons = {first, second};

    WeaponRegistry registry;
    CHECK(registry.Configure(scene));
    CHECK(registry.Count() == 2);
    CHECK(registry.Active().id == "weapon_000");
    registry.Cycle(1);
    CHECK(registry.Active().id == "weapon_002");
    registry.Cycle(1);
    CHECK(registry.Active().id == "weapon_000");
    registry.Cycle(-1);
    CHECK(registry.Active().id == "weapon_002");
    CHECK(registry.Select(0));
    CHECK(registry.Active().id == "weapon_000");
    CHECK_FALSE(registry.Select(9));
}

TEST_CASE("Developer enemy spawner is deterministic and reset restores fixtures" *
          doctest::test_suite("unit")) {
    SceneAsset scene;
    EnemyDefinition enemy;
    enemy.id = "e_slime01";
    enemy.hp = 12;
    scene.enemy_definitions.push_back(enemy);
    EnemyFixture fixture;
    fixture.definition = "e_slime01";
    fixture.position = {4.0f, 5.0f};
    scene.enemy_fixtures.push_back(fixture);

    EnemySystem enemies;
    std::string error;
    CHECK(enemies.Configure(scene, &error));
    CHECK(error.empty());
    CHECK(enemies.Enemies().size() == 1);
    CHECK(enemies.SpawnEnemy(0, {8.0f, 9.0f}));
    CHECK(enemies.Enemies().size() == 2);
    CHECK(enemies.Enemies().back().position.x == doctest::Approx(8.0f));
    enemies.ClearEnemies();
    CHECK(enemies.Enemies().empty());
    enemies.Reset();
    CHECK(enemies.Enemies().size() == 1);
    CHECK(enemies.Enemies()[0].position.x == doctest::Approx(4.0f));
    CHECK(enemies.Enemies()[0].health.hp == 12);
}
