// Soul Knight Linux — weapon, projectile, damage and enemy combat tests
#include <doctest/doctest.h>

#include "engine/assets/SceneAsset.hpp"
#include "engine/collision/AabbCollision.hpp"
#include "engine/locomotion/PlayerLocomotion.hpp"
#include "game/combat/Damage.hpp"
#include "game/combat/ProjectileSystem.hpp"
#include "game/combat/Weapon.hpp"
#include "game/enemy/EnemySystem.hpp"

#include <cmath>

using soulknight::engine::Vec2;
using soulknight::engine::assets::EnemyDefinition;
using soulknight::engine::assets::EnemyFixture;
using soulknight::engine::assets::ProjectileDefinition;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::assets::WeaponDefinition;
using soulknight::engine::collision::Aabb;
using soulknight::engine::collision::AabbCollisionWorld;
using soulknight::game::combat::DamageEvent;
using soulknight::game::combat::HealthState;
using soulknight::game::combat::ProjectileSystem;
using soulknight::game::combat::Weapon;
using soulknight::engine::locomotion::LocomotionConfig;
using soulknight::game::enemy::EnemySystem;

namespace {

WeaponDefinition MakeWeapon(float attack_cycle = 1.0f / 3.0f,
                            float first_shot = 1.0f / 30.0f) {
    WeaponDefinition weapon;
    weapon.id = "weapon_000";
    weapon.bullet_speed = 36.0f;
    weapon.size = 1.0f;
    weapon.damage = 3;
    weapon.deviation = 5;
    weapon.owner_deviation = 0;
    weapon.consume = 0;
    weapon.weapon_speed = 1.0f;
    weapon.repel = 2;
    weapon.through = 0;
    weapon.attack_cycle_seconds = attack_cycle;
    weapon.first_shot_delay_seconds = first_shot;
    weapon.projectile.id = "bullet_0";
    weapon.projectile.sprite_index = 0;
    weapon.projectile.prefab_speed = 10.0f;
    weapon.projectile.lifetime = 5.0f;
    weapon.projectile.camp = 1;
    weapon.projectile.can_rebound = true;
    weapon.projectile.collider_half_extents = {0.25f, 0.20f};
    weapon.projectile.collider_offset = {0.25f, 0.0f};
    return weapon;
}

EnemyDefinition MakeEnemy() {
    EnemyDefinition enemy;
    enemy.id = "e_slime01";
    enemy.body_sprite_index = 0;
    enemy.dead_sprite_index = 1;
    enemy.shadow_sprite_index = 2;
    enemy.hp = 12;
    enemy.camp = 0;
    enemy.contact_damage = 3;
    enemy.contact_cooldown = 1.5f;
    enemy.death_despawn_delay = 4.0f;
    enemy.collider.min = {-0.5f, 0.0f};
    enemy.collider.max = {0.5f, 1.0f};
    return enemy;
}

SceneAsset MakeEnemyScene() {
    SceneAsset scene;
    scene.enemy_definitions.push_back(MakeEnemy());
    EnemyFixture fixture;
    fixture.definition = "e_slime01";
    fixture.position = {10.0f, 5.0f};
    scene.enemy_fixtures.push_back(fixture);
    return scene;
}

int TickWeapon(Weapon& weapon, int ticks, bool held,
               const Vec2& owner, const Vec2& aim,
               ProjectileSystem& projectiles, float dt = 0.02f) {
    int fired = 0;
    for (int i = 0; i < ticks; ++i) {
        fired += weapon.Update(dt, held, owner, aim, projectiles);
    }
    return fired;
}

} // namespace

TEST_CASE("Weapon_000 follows the recovered Animator attack cycle" *
          doctest::test_suite("unit")) {
    Weapon weapon;
    const WeaponDefinition definition = MakeWeapon();
    CHECK(weapon.Configure(definition));
    ProjectileSystem projectiles;
    const Vec2 owner{5.0f, 5.0f};

    // The w_pistol Attack event is at 1/30 s. Two fixed 0.02 s ticks reach
    // 0.04 s, so the first shot appears on the second tick.
    CHECK(TickWeapon(weapon, 1, true, owner, {1.0f, 0.0f}, projectiles) == 0);
    CHECK(projectiles.Projectiles().empty());
    CHECK(weapon.Update(0.02f, true, owner, {1.0f, 0.0f}, projectiles) == 1);
    CHECK(projectiles.Projectiles().size() == 1);
    const auto& projectile = projectiles.Projectiles()[0];
    CHECK(projectile.active);
    CHECK(projectile.damage == 3);
    CHECK(projectile.camp == 1);
    CHECK(projectile.through == 0);
    CHECK(projectile.repel == 2);
    CHECK(projectile.lifetime == doctest::Approx(5.0f));
    CHECK(projectile.velocity.Length() == doctest::Approx(36.0f));
    // Muzzle composition: c00 hand (0,0.5) + gun_point (0.6,0.15).
    CHECK(projectile.position.x == doctest::Approx(5.6f));
    CHECK(projectile.position.y == doctest::Approx(5.65f));
    // The prefab default 10 is preserved but overridden by BulletInfo 36.
    CHECK(definition.projectile.prefab_speed == doctest::Approx(10.0f));

    // weapon_000 spread is weapon.deviation * (attribute.deviation + 1).
    // c00 attribute.deviation is 0, so both shots use the exact same aim.
    CHECK(TickWeapon(weapon, 16, true, owner, {1.0f, 0.0f}, projectiles) == 0);
    CHECK(weapon.Update(0.02f, true, owner, {1.0f, 0.0f}, projectiles) == 1);
    CHECK(projectiles.Projectiles().size() == 2);
    CHECK(projectiles.Projectiles()[1].velocity.x ==
          doctest::Approx(projectiles.Projectiles()[0].velocity.x));
    CHECK(projectiles.Projectiles()[1].velocity.y ==
          doctest::Approx(projectiles.Projectiles()[0].velocity.y));
}

TEST_CASE("Weapon attack cycle is one shot per 1/3 s and release resets it" *
          doctest::test_suite("unit")) {
    Weapon weapon;
    CHECK(weapon.Configure(MakeWeapon()));
    ProjectileSystem projectiles;
    const Vec2 owner{0.0f, 0.0f};

    // 3 seconds of held attack at 50 Hz: first event at t=1/30, then every
    // 1/3 s => events at 0.0333, 0.3666, 0.7, 1.0333, 1.3666, 1.7, 2.0333,
    // 2.3666, 2.7, 3.0333. Nine events fall inside 3.0 s.
    const int fired = TickWeapon(weapon, 150, true, owner, {1.0f, 0.0f},
                                 projectiles);
    CHECK(fired == 9);
    CHECK(projectiles.Projectiles().size() == 9);

    // Releasing stops the state; re-pressing restarts the 1/30 s first shot.
    weapon.Update(0.02f, false, owner, {1.0f, 0.0f}, projectiles);
    CHECK_FALSE(weapon.IsAttacking());
    CHECK(weapon.Update(0.02f, true, owner, {1.0f, 0.0f}, projectiles) == 0);
    CHECK(weapon.Update(0.02f, true, owner, {1.0f, 0.0f}, projectiles) == 1);
}

TEST_CASE("Weapon rejects zero-duration cycles and invalid configuration" *
          doctest::test_suite("unit")) {
    Weapon weapon;
    WeaponDefinition definition = MakeWeapon(0.0f);
    CHECK_FALSE(weapon.Configure(definition));
    CHECK_FALSE(weapon.IsAttacking());
}

TEST_CASE("Projectiles move deterministically and expire at five seconds" *
          doctest::test_suite("unit")) {
    ProjectileSystem projectiles;
    const ProjectileDefinition definition = MakeWeapon().projectile;
    projectiles.Spawn(definition, {2.0f, 3.0f}, {1.0f, 0.0f},
                      36.0f, 3, 1, 0, 2);
    AabbCollisionWorld world;
    world.SetBounds({{0.0f, 0.0f}, {41.0f, 41.0f}});

    const Vec2 start = projectiles.Projectiles()[0].position;
    projectiles.FixedUpdate(0.02f, world);
    CHECK(projectiles.Projectiles()[0].position.x ==
          doctest::Approx(start.x + 0.72f));
    CHECK(projectiles.Projectiles()[0].position.y == doctest::Approx(start.y));

    for (int i = 0; i < 249; ++i) {
        projectiles.FixedUpdate(0.02f, world);
    }
    CHECK(projectiles.Projectiles()[0].age == doctest::Approx(5.0f));
    CHECK_FALSE(projectiles.Projectiles()[0].active);
    CHECK(projectiles.ActiveCount() == 0);
}

TEST_CASE("Projectiles stop at walls and obstacles instead of passing through" *
          doctest::test_suite("unit")) {
    AabbCollisionWorld world;
    world.SetBounds({{0.0f, 0.0f}, {41.0f, 41.0f}});
    world.AddSolid({{5.0f, 0.0f}, {5.5f, 10.0f}});
    ProjectileSystem projectiles;
    projectiles.Spawn(MakeWeapon().projectile, {2.0f, 5.0f}, {1.0f, 0.0f},
                      36.0f, 3, 1, 0, 2);
    for (int i = 0; i < 20; ++i) {
        projectiles.FixedUpdate(0.02f, world);
    }
    const auto& projectile = projectiles.Projectiles()[0];
    CHECK_FALSE(projectile.active);
    CHECK(projectile.position.x <= 5.25f + 1e-4f);
    CHECK(projectile.position.y == doctest::Approx(5.0f));

    AabbCollisionWorld obstacle_world;
    obstacle_world.SetBounds({{0.0f, 0.0f}, {41.0f, 41.0f}});
    obstacle_world.AddSolid({{4.0f, 4.0f}, {5.0f, 6.0f}});
    ProjectileSystem obstacles;
    obstacles.Spawn(MakeWeapon().projectile, {2.0f, 5.0f}, {1.0f, 0.0f},
                    36.0f, 3, 1, 0, 2);
    for (int i = 0; i < 20; ++i) {
        obstacles.FixedUpdate(0.02f, obstacle_world);
    }
    CHECK_FALSE(obstacles.Projectiles()[0].active);
    CHECK(obstacles.Projectiles()[0].position.x <= 3.75f + 1e-4f);
}

TEST_CASE("Damage pipeline handles armour overflow, defence and lethal hits" *
          doctest::test_suite("unit")) {
    HealthState player;
    player.camp = 1;
    player.Reset(6, 5, 0);
    DamageEvent event;
    event.damage = 3;
    event.camp = 0;
    CHECK(player.ApplyDamage(event));
    CHECK(player.armor == 2);
    CHECK(player.hp == 6);
    CHECK(player.ApplyDamage(event));
    CHECK(player.armor == 0);
    CHECK(player.hp == 5);
    CHECK(player.ApplyDamage(event));
    CHECK(player.hp == 2);
    CHECK(player.Alive());

    HealthState defended;
    defended.camp = 1;
    defended.Reset(6, 0, 2);
    DamageEvent defended_event;
    defended_event.damage = 3;
    defended_event.camp = 0;
    CHECK(defended.ApplyDamage(defended_event));
    CHECK(defended.hp == 5);

    // Same camp does not damage.
    CHECK_FALSE(player.ApplyDamage(DamageEvent{100, 1}));
    CHECK(player.Alive());

    DamageEvent lethal;
    lethal.damage = 100;
    lethal.camp = 0;
    CHECK(player.ApplyDamage(lethal));
    CHECK(player.hp == 0);
    CHECK(player.dead);
    CHECK_FALSE(player.Alive());
    CHECK_FALSE(player.ApplyDamage({1, 0}));
}

TEST_CASE("Enemy receives projectile damage, dies and disables collision" *
          doctest::test_suite("unit")) {
    const SceneAsset scene = MakeEnemyScene();
    EnemySystem enemies;
    std::string error;
    CHECK(enemies.Configure(scene, &error));
    CHECK(error.empty());
    CHECK(enemies.Enemies().size() == 1);
    CHECK(enemies.Enemies()[0].health.hp == 12);
    CHECK(enemies.Enemies()[0].health.camp == 0);

    ProjectileSystem projectiles;
    projectiles.Spawn(MakeWeapon().projectile, {9.5f, 5.5f}, {1.0f, 0.0f},
                      36.0f, 3, 1, 0, 2);
    enemies.ResolveProjectileHits(projectiles);
    CHECK(enemies.Enemies()[0].health.hp == 9);
    CHECK_FALSE(projectiles.Projectiles()[0].active);
    CHECK_FALSE(enemies.Enemies()[0].health.dead);

    for (int i = 0; i < 3; ++i) {
        projectiles.Spawn(MakeWeapon().projectile, {9.5f, 5.5f},
                          {1.0f, 0.0f}, 36.0f, 3, 1, 0, 2);
        enemies.ResolveProjectileHits(projectiles);
    }
    CHECK(enemies.Enemies()[0].health.hp == 0);
    CHECK(enemies.Enemies()[0].health.dead);
    CHECK(enemies.AliveCount() == 0);
    CHECK(enemies.Enemies()[0].death_timer == doctest::Approx(4.0f));

    // Dying enemies cannot be hit again and their collider is ignored.
    projectiles.Spawn(MakeWeapon().projectile, {9.5f, 5.5f}, {1.0f, 0.0f},
                      36.0f, 3, 1, 0, 2);
    enemies.ResolveProjectileHits(projectiles);
    CHECK(projectiles.Projectiles().back().active);

    HealthState player;
    player.camp = 1;
    player.Reset(6, 5, 0);
    enemies.FixedUpdate(3.99f, {10.0f, 5.0f}, player);
    CHECK(enemies.Enemies()[0].active);
    enemies.FixedUpdate(0.02f, {10.0f, 5.0f}, player);
    CHECK_FALSE(enemies.Enemies()[0].active);
}

TEST_CASE("Enemy contact damage uses the recovered 3 / 1.5 s cadence" *
          doctest::test_suite("unit")) {
    const SceneAsset scene = MakeEnemyScene();
    EnemySystem enemies;
    std::string error;
    CHECK(enemies.Configure(scene, &error));
    HealthState player;
    player.camp = 1;
    player.Reset(6, 5, 0);

    CHECK(enemies.FixedUpdate(0.02f, {10.0f, 5.0f}, player));
    CHECK(player.armor == 2);
    CHECK_FALSE(enemies.FixedUpdate(0.02f, {10.0f, 5.0f}, player));
    CHECK(player.armor == 2);
    enemies.FixedUpdate(1.5f, {10.0f, 5.0f}, player);
    CHECK(player.armor == 0);
    CHECK(player.hp == 5);

    CHECK_FALSE(enemies.FixedUpdate(0.02f, {20.0f, 20.0f}, player));
}

TEST_CASE("Projectile targets behind a wall are not damaged" *
          doctest::test_suite("unit")) {
    const SceneAsset scene = MakeEnemyScene();
    EnemySystem enemies;
    std::string error;
    CHECK(enemies.Configure(scene, &error));

    // Simulate a projectile whose sweep ended at a wall at 30% of the segment.
    // The enemy at x=10 lies behind that wall hit point and must be ignored.
    ProjectileSystem behind;
    behind.Spawn(MakeWeapon().projectile, {8.0f, 5.5f}, {1.0f, 0.0f},
                 36.0f, 3, 1, 0, 2);
    auto& behind_projectile = behind.Projectiles()[0];
    behind_projectile.previous_position = {8.0f, 5.5f};
    behind_projectile.position = {11.0f, 5.5f};
    behind_projectile.active = false;
    behind_projectile.hit_world_this_step = true;
    behind_projectile.world_hit_time = 0.3f;
    enemies.ResolveProjectileHits(behind);
    CHECK(enemies.Enemies()[0].health.hp == 12);

    // The same wall hit does not mask an enemy in front of the wall.
    ProjectileSystem front;
    front.Spawn(MakeWeapon().projectile, {7.0f, 5.5f}, {1.0f, 0.0f},
                36.0f, 3, 1, 0, 2);
    auto& front_projectile = front.Projectiles()[0];
    front_projectile.previous_position = {7.0f, 5.5f};
    front_projectile.position = {11.0f, 5.5f};
    front_projectile.active = false;
    front_projectile.hit_world_this_step = true;
    front_projectile.world_hit_time = 0.9f;
    enemies.ResolveProjectileHits(front);
    CHECK(enemies.Enemies()[0].health.hp == 9);
}

TEST_CASE("Weapon atk_move_speed slows movement while attack is held" *
          doctest::test_suite("unit")) {
    LocomotionConfig config;
    CHECK(config.EffectiveSpeed() == doctest::Approx(9.75f));
    config.speed_rate_bonus = -0.1f;
    CHECK(config.EffectiveSpeed() == doctest::Approx(9.1f));
}

TEST_CASE("Projectile collider offset rotates with velocity" *
          doctest::test_suite("unit")) {
    ProjectileSystem projectiles;
    projectiles.Spawn(MakeWeapon().projectile, {1.0f, 2.0f}, {1.0f, 0.0f},
                      36.0f, 3, 1, 0, 2);
    auto& projectile = projectiles.Projectiles()[0];
    Vec2 offset = ProjectileSystem::RotatedColliderOffset(projectile);
    CHECK(offset.x == doctest::Approx(0.25f));
    CHECK(offset.y == doctest::Approx(0.0f));

    projectile.velocity = {0.0f, 36.0f};
    offset = ProjectileSystem::RotatedColliderOffset(projectile);
    CHECK(offset.x == doctest::Approx(0.0f));
    CHECK(offset.y == doctest::Approx(0.25f));
}

TEST_CASE("Recovered multiCount weapons spawn multiple projectiles per Attack" *
          doctest::test_suite("unit")) {
    auto definition = MakeWeapon();
    definition.bullet_count = 5;
    definition.spread_degrees = 20;
    definition.attack_cycle_seconds = 0.8f;
    definition.first_shot_delay_seconds = 0.0f;
    Weapon weapon;
    CHECK(weapon.Configure(definition));
    ProjectileSystem projectiles;
    CHECK(weapon.Update(0.01f, true, {0.0f, 0.0f}, {1.0f, 0.0f},
                        projectiles) == 1);
    CHECK(projectiles.Projectiles().size() == 5);
    for (const auto& projectile : projectiles.Projectiles()) {
        CHECK(projectile.damage == definition.damage);
        CHECK(projectile.through == definition.through);
        CHECK(projectile.repel == definition.repel);
    }
}

TEST_CASE("Generic melee weapon uses frontal arc without spawning bullets" *
          doctest::test_suite("unit")) {
    auto definition = MakeWeapon(0.5f, 0.0f);
    definition.is_melee = true;
    definition.bullet_speed = 0.0f;
    definition.melee_range = 2.0f;
    definition.melee_arc_degrees = 120.0f;
    Weapon weapon;
    CHECK(weapon.Configure(definition));
    ProjectileSystem projectiles;
    CHECK(weapon.Update(0.01f, true, {0.0f, 0.0f}, {1.0f, 0.0f},
                        projectiles) == 1);
    CHECK(projectiles.Projectiles().empty());
}

TEST_CASE("Enemy melee arc damages only targets in front" *
          doctest::test_suite("unit")) {
    SceneAsset scene = MakeEnemyScene();
    EnemySystem enemies;
    std::string error;
    CHECK(enemies.Configure(scene, &error));
    enemies.ClearEnemies();
    CHECK(enemies.SpawnEnemy(0, {1.0f, 0.0f}));
    CHECK(enemies.SpawnEnemy(0, {-1.0f, 0.0f}));
    CHECK(enemies.ApplyMelee({0.0f, 0.0f}, {1.0f, 0.0f},
                             2.0f, 120.0f, 4, 1) == 1);
    CHECK(enemies.Enemies()[0].health.hp < 12);
    CHECK(enemies.Enemies()[1].health.hp == 12);
}
