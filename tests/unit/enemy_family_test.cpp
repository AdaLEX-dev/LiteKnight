// Soul Knight Linux — recovered enemy controller-family tests
#include <doctest/doctest.h>

#include "engine/assets/SceneAsset.hpp"
#include "game/combat/ProjectileSystem.hpp"
#include "game/enemy/EnemySystem.hpp"

using soulknight::engine::Vec2;
using soulknight::engine::assets::EnemyDefinition;
using soulknight::engine::assets::SceneAsset;
using soulknight::game::combat::HealthState;
using soulknight::game::combat::ProjectileSystem;
using soulknight::game::enemy::EnemySystem;

namespace {

EnemyDefinition BaseEnemy() {
    EnemyDefinition definition;
    definition.id = "e_test";
    definition.hp = 20;
    definition.speed = 0;
    definition.camp = 0;
    definition.contact_damage = 0;
    definition.collider = {{-0.25f, 0.0f}, {0.25f, 0.8f}};
    definition.projectile.sprite_index = 0;
    definition.projectile.lifetime = 5.0f;
    definition.projectile.prefab_speed = 10.0f;
    definition.projectile.collider_half_extents = {0.1f, 0.1f};
    definition.projectile.collider_offset = {0.0f, 0.0f};
    definition.projectile_speed = 10.0f;
    definition.projectile_damage = 2;
    definition.attack_range = 10.0f;
    definition.shoot_cooldown = 1.0f;
    return definition;
}

EnemySystem Configured(const EnemyDefinition& definition,
                       const Vec2& position) {
    SceneAsset scene;
    scene.enemy_definitions.push_back(definition);
    EnemySystem enemies;
    std::string error;
    CHECK(enemies.Configure(scene, &error));
    CHECK(enemies.SpawnEnemy(0, position));
    return enemies;
}

} // namespace

TEST_CASE("EnemyAI20 fan emits the recovered 2*bulletCount+1 projectiles" *
          doctest::test_suite("unit")) {
    EnemyDefinition definition = BaseEnemy();
    definition.ranged = true;
    definition.projectile_count = 5;
    definition.fan_angle = 15.0f;
    EnemySystem enemies = Configured(definition, Vec2{});
    ProjectileSystem projectiles;
    HealthState player;
    player.Reset(100);
    player.camp = 1;
    (void)enemies.FixedUpdate(0.02f, Vec2{5.0f, 0.0f}, player, nullptr,
                              &projectiles);
    CHECK(projectiles.Projectiles().size() == 5);
    CHECK(projectiles.Projectiles()[0].velocity.y < 0.0f);
    CHECK(projectiles.Projectiles()[4].velocity.y > 0.0f);
}

TEST_CASE("EnemyAI11/14 burst schedules its recovered projectile count" *
          doctest::test_suite("unit")) {
    EnemyDefinition definition = BaseEnemy();
    definition.ranged = true;
    definition.projectile_count = 1;
    definition.burst_count = 5;
    definition.burst_interval = 0.1f;
    EnemySystem enemies = Configured(definition, Vec2{});
    ProjectileSystem projectiles;
    HealthState player;
    player.Reset(100);
    player.camp = 1;
    for (int tick = 0; tick < 30; ++tick) { // 0.6 s
        (void)enemies.FixedUpdate(0.02f, Vec2{5.0f, 0.0f}, player, nullptr,
                                  &projectiles);
    }
    CHECK(projectiles.Projectiles().size() == 5);
}

TEST_CASE("EnemyAI09/Witch dash moves at the recovered speed window" *
          doctest::test_suite("unit")) {
    EnemyDefinition definition = BaseEnemy();
    definition.dash_speed = 8.0f;
    definition.dash_duration = 0.5f;
    definition.attack_range = 6.0f;
    EnemySystem enemies = Configured(definition, Vec2{});
    ProjectileSystem projectiles;
    HealthState player;
    player.Reset(100);
    player.camp = 1;
    (void)enemies.FixedUpdate(0.1f, Vec2{4.0f, 0.0f}, player, nullptr,
                              &projectiles);
    CHECK(enemies.Enemies()[0].position.x > 0.5f);
    CHECK(enemies.Enemies()[0].dashing);
    for (int tick = 0; tick < 30; ++tick) {
        (void)enemies.FixedUpdate(0.02f, Vec2{4.0f, 0.0f}, player, nullptr,
                                  &projectiles);
    }
    CHECK_FALSE(enemies.Enemies()[0].dashing);
}

TEST_CASE("EnemyAI13 death explosion applies its recovered area damage" *
          doctest::test_suite("unit")) {
    EnemyDefinition definition = BaseEnemy();
    definition.death_explosion_radius = 3.0f;
    definition.death_explosion_damage = 5;
    EnemySystem enemies = Configured(definition, Vec2{});
    HealthState player;
    player.Reset(100);
    player.camp = 1;
    CHECK(enemies.ApplyDamage(0, 999, 1));
    (void)enemies.FixedUpdate(0.02f, Vec2{1.0f, 0.0f}, player, nullptr,
                              nullptr);
    CHECK(player.hp == 95);
}

TEST_CASE("EnemyAI16 attack window is invulnerable" *
          doctest::test_suite("unit")) {
    EnemyDefinition definition = BaseEnemy();
    definition.ranged = true;
    definition.stationary = true;
    definition.invulnerable_while_attacking = true;
    definition.shoot_cooldown = 3.0f;
    EnemySystem enemies = Configured(definition, Vec2{});
    ProjectileSystem projectiles;
    HealthState player;
    player.Reset(100);
    player.camp = 1;
    (void)enemies.FixedUpdate(0.02f, Vec2{5.0f, 0.0f}, player, nullptr,
                              &projectiles);
    CHECK_FALSE(enemies.ApplyDamage(0, 5, 1));
    for (int tick = 0; tick < 100; ++tick) {
        (void)enemies.FixedUpdate(0.02f, Vec2{5.0f, 0.0f}, player, nullptr,
                                  &projectiles);
    }
    CHECK(enemies.ApplyDamage(0, 5, 1));
}

TEST_CASE("Stationary controllers do not chase but still fire" *
          doctest::test_suite("unit")) {
    EnemyDefinition definition = BaseEnemy();
    definition.ranged = true;
    definition.stationary = true;
    definition.speed = 10;
    EnemySystem enemies = Configured(definition, Vec2{});
    ProjectileSystem projectiles;
    HealthState player;
    player.Reset(100);
    player.camp = 1;
    (void)enemies.FixedUpdate(0.5f, Vec2{5.0f, 0.0f}, player, nullptr,
                              &projectiles);
    CHECK(enemies.Enemies()[0].position.x == doctest::Approx(0.0f));
    CHECK(projectiles.Projectiles().size() == 1);
}
