// Soul Knight Linux — deterministic first-slice enemy pursuit tests
#include <doctest/doctest.h>

#include "engine/assets/SceneAsset.hpp"
#include "engine/collision/AabbCollision.hpp"
#include "game/combat/Damage.hpp"
#include "game/enemy/EnemySystem.hpp"

using soulknight::engine::Vec2;
using soulknight::engine::assets::EnemyDefinition;
using soulknight::engine::assets::EnemyFixture;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::collision::Aabb;
using soulknight::engine::collision::AabbCollisionWorld;
using soulknight::game::combat::HealthState;
using soulknight::game::enemy::EnemySystem;

namespace {

EnemyDefinition MakeSlime() {
    EnemyDefinition enemy;
    enemy.id = "e_slime01";
    enemy.hp = 12;
    enemy.speed = 3;
    enemy.camp = 0;
    enemy.contact_damage = 3;
    enemy.contact_cooldown = 1.5f;
    enemy.collider.min = {-0.5f, 0.0f};
    enemy.collider.max = {0.5f, 1.0f};
    return enemy;
}

SceneAsset MakeScene() {
    SceneAsset scene;
    scene.enemy_definitions.push_back(MakeSlime());
    EnemyFixture fixture;
    fixture.definition = "e_slime01";
    fixture.position = {4.0f, 0.0f};
    scene.enemy_fixtures.push_back(fixture);
    return scene;
}

void Tick(EnemySystem& enemies, int ticks, const Vec2& player,
          HealthState& health, const AabbCollisionWorld* world = nullptr,
          float dt = 0.02f) {
    for (int i = 0; i < ticks; ++i) {
        (void)enemies.FixedUpdate(dt, player, health, world);
    }
}

} // namespace

TEST_CASE("Enemy pursues the player at its recovered speed" *
          doctest::test_suite("unit")) {
    EnemySystem enemies;
    SceneAsset scene = MakeScene();
    CHECK(enemies.Configure(scene, nullptr));
    HealthState player;
    player.Reset(6, 0, 0);
    player.camp = 1;

    for (int i = 0; i < 25; ++i) {
        (void)enemies.FixedUpdate(0.02f, Vec2{0.0f, 0.0f}, player);
    }
    CHECK(enemies.Enemies().size() == 1);
    const float x = enemies.Enemies()[0].position.x;
    // Unobstructed direct pursuit: 0.5 s at 3 units/s from x=4.
    CHECK(x == doctest::Approx(2.5f));
    CHECK(enemies.Enemies()[0].facing_left);
}

TEST_CASE("Enemy ignores a player outside the first-slice aggro radius" *
          doctest::test_suite("unit")) {
    EnemySystem enemies;
    SceneAsset scene = MakeScene();
    scene.enemy_fixtures[0].position = {40.0f, 0.0f};
    CHECK(enemies.Configure(scene, nullptr));
    HealthState player;
    player.Reset(6, 0, 0);
    player.camp = 1;
    Tick(enemies, 50, Vec2{0.0f, 0.0f}, player);
    CHECK(enemies.Enemies()[0].position.x == doctest::Approx(40.0f));
}

TEST_CASE("Enemy reaches contact range and deals cooldown-gated damage" *
          doctest::test_suite("unit")) {
    EnemySystem enemies;
    SceneAsset scene = MakeScene();
    scene.enemy_fixtures[0].position = {1.0f, 0.0f};
    CHECK(enemies.Configure(scene, nullptr));
    HealthState player;
    player.Reset(20, 0, 0);
    player.camp = 1;

    Tick(enemies, 1, Vec2{0.0f, 0.0f}, player);
    CHECK(player.hp == 20);
    Tick(enemies, 4, Vec2{0.0f, 0.0f}, player);
    CHECK(player.hp == 17);
    CHECK(player.armor == 0);
    // Contact cooldown blocks immediate repeat damage.
    Tick(enemies, 10, Vec2{0.0f, 0.0f}, player);
    CHECK(player.hp == 17);
    Tick(enemies, 70, Vec2{0.0f, 0.0f}, player);
    CHECK(player.hp == 14);
}

TEST_CASE("Enemy collision sliding stops pursuit at a solid wall" *
          doctest::test_suite("unit")) {
    EnemySystem enemies;
    SceneAsset scene = MakeScene();
    scene.enemy_fixtures[0].position = {0.0f, 0.0f};
    CHECK(enemies.Configure(scene, nullptr));
    HealthState player;
    player.Reset(20, 0, 0);
    player.camp = 1;
    AabbCollisionWorld world;
    world.SetBounds(Aabb{{-20.0f, -20.0f}, {20.0f, 20.0f}});
    world.AddSolid(Aabb{{4.0f, -2.0f}, {4.5f, 2.0f}});

    Tick(enemies, 200, Vec2{4.5f, 0.0f}, player, &world);
    // Collider extends from x-0.5 to x+0.5; the wall begins at x=4.
    CHECK(enemies.Enemies()[0].position.x <= 3.51f);
    CHECK(enemies.Enemies()[0].position.x > 3.0f);
}

TEST_CASE("Dead enemies stop pursuing and stop contact damage" *
          doctest::test_suite("unit")) {
    EnemySystem enemies;
    SceneAsset scene = MakeScene();
    scene.enemy_fixtures[0].position = {1.0f, 0.0f};
    CHECK(enemies.Configure(scene, nullptr));
    HealthState player;
    player.Reset(20, 0, 0);
    player.camp = 1;

    CHECK(enemies.ApplyDamage(0, 999, 1));
    CHECK(enemies.Enemies()[0].health.dead);
    const Vec2 before = enemies.Enemies()[0].position;
    Tick(enemies, 20, Vec2{0.0f, 0.0f}, player);
    CHECK(enemies.Enemies()[0].position.x == doctest::Approx(before.x));
    CHECK(player.hp == 20);
    CHECK(enemies.AliveCount() == 0);
}
