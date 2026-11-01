// Soul Knight Linux — recovered unique-weapon mechanics tests
#include <doctest/doctest.h>

#include "engine/assets/SceneAsset.hpp"
#include "game/combat/Weapon.hpp"
#include "game/enemy/EnemySystem.hpp"
#include "game/summon/SummonSystem.hpp"

using soulknight::engine::Vec2;
using soulknight::engine::assets::EnemyDefinition;
using soulknight::engine::assets::ProjectileDefinition;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::assets::WeaponDefinition;
using soulknight::engine::assets::WeaponProjectileVariant;
using soulknight::engine::assets::WeaponSpecial;
using soulknight::game::combat::ProjectileSystem;
using soulknight::game::combat::Weapon;
using soulknight::game::enemy::EnemySystem;
using soulknight::game::summon::SummonSystem;

namespace {

WeaponDefinition ProjectileWeapon() {
    WeaponDefinition definition;
    definition.id = "test";
    definition.bullet_speed = 10.0f;
    definition.projectile.lifetime = 5.0f;
    definition.projectile.prefab_speed = 10.0f;
    definition.projectile.collider_half_extents = {0.1f, 0.1f};
    definition.projectile.collider_offset = {0.0f, 0.0f};
    definition.damage = 1;
    definition.attack_cycle_seconds = 1.0f;
    definition.first_shot_delay_seconds = 0.0f;
    definition.consume = 0;
    return definition;
}

EnemySystem MakeEnemySystem() {
    SceneAsset scene;
    EnemyDefinition definition;
    definition.id = "e_test";
    definition.hp = 10;
    definition.speed = 0;
    definition.camp = 0;
    definition.contact_damage = 0;
    definition.collider = {{-0.25f, 0.0f}, {0.25f, 0.8f}};
    scene.enemy_definitions.push_back(definition);
    EnemySystem enemies;
    std::string error;
    CHECK(enemies.Configure(scene, &error));
    return enemies;
}

} // namespace

TEST_CASE("Recovered animation burst fires every serialized Attack event" *
          doctest::test_suite("unit")) {
    WeaponDefinition definition = ProjectileWeapon();
    definition.attack_cycle_seconds = 2.0f;
    definition.attack_event_times = {0.1f, 0.4f, 0.7667f};
    Weapon weapon;
    CHECK(weapon.Configure(definition));
    ProjectileSystem projectiles;
    int fired = 0;
    for (int tick = 0; tick < 40; ++tick) { // 0.8 s at 50 Hz
        fired += weapon.Update(0.02f, true, Vec2{}, Vec2{1.0f, 0.0f},
                               projectiles);
    }
    CHECK(fired == 3);
    CHECK(projectiles.ActiveCount() == 3);

    // The next cycle repeats the same three events.
    for (int tick = 0; tick < 70; ++tick) { // to 2.2 s
        fired += weapon.Update(0.02f, true, Vec2{}, Vec2{1.0f, 0.0f},
                               projectiles);
    }
    CHECK(fired == 4);
}

TEST_CASE("Gun008 burst count and interval are recovered values" *
          doctest::test_suite("unit")) {
    WeaponDefinition definition = ProjectileWeapon();
    definition.attack_cycle_seconds = 1.0f;
    definition.special.kind = "revolver_burst";
    definition.special.count = 6;
    definition.special.interval = 0.12f;
    definition.attack_event_times = {0.0333f, 0.1533f, 0.2733f,
                                     0.3933f, 0.5133f, 0.6333f};
    WeaponProjectileVariant variant;
    variant.projectile.lifetime = 5.0f;
    variant.projectile.prefab_speed = 10.0f;
    variant.projectile.collider_half_extents = {0.1f, 0.1f};
    variant.projectile.collider_offset = {0.0f, 0.0f};
    variant.bullet_speed = 10.0f;
    variant.damage = 4;
    definition.special.variants.push_back(variant);
    Weapon weapon;
    CHECK(weapon.Configure(definition));
    ProjectileSystem projectiles;
    int fired = 0;
    for (int tick = 0; tick < 35; ++tick) { // 0.7 s
        fired += weapon.Update(0.02f, true, Vec2{}, Vec2{1.0f, 0.0f},
                               projectiles);
    }
    CHECK(fired == 6);
    CHECK(projectiles.ActiveCount() == 6);
}

TEST_CASE("GunMultiBullet02 cycles variants with the recovered angle step" *
          doctest::test_suite("unit")) {
    WeaponDefinition definition = ProjectileWeapon();
    definition.special.kind = "multi_pattern";
    definition.special.count = 4;
    definition.special.value = 8.0f;
    for (int i = 0; i < 2; ++i) {
        WeaponProjectileVariant variant;
        variant.projectile.lifetime = 5.0f;
        variant.projectile.prefab_speed = 10.0f;
        variant.projectile.collider_half_extents = {0.1f, 0.1f};
        variant.projectile.collider_offset = {0.0f, 0.0f};
        variant.bullet_speed = 10.0f;
        variant.damage = 4 + i;
        definition.special.variants.push_back(variant);
    }
    Weapon weapon;
    CHECK(weapon.Configure(definition));
    ProjectileSystem projectiles;
    CHECK(weapon.Update(0.02f, true, Vec2{}, Vec2{1.0f, 0.0f},
                        projectiles) == 1);
    CHECK(projectiles.Projectiles().size() == 4);
    // Center pair straddles the aim direction: -8 deg, 0 deg for even count.
    CHECK(projectiles.Projectiles()[0].velocity.y < 0.0f);
    CHECK(projectiles.Projectiles()[3].velocity.y > 0.0f);
    CHECK(projectiles.Projectiles()[0].damage == 4);
    CHECK(projectiles.Projectiles()[1].damage == 5);
    CHECK(projectiles.Projectiles()[2].damage == 4);
    CHECK(projectiles.Projectiles()[3].damage == 5);
}

TEST_CASE("Attack-speed buff compresses the recovered attack cycle" *
          doctest::test_suite("unit")) {
    WeaponDefinition definition = ProjectileWeapon();
    definition.attack_cycle_seconds = 1.0f;
    Weapon weapon;
    CHECK(weapon.Configure(definition));
    weapon.SetAttackSpeedMultiplier(2.0f);
    ProjectileSystem projectiles;
    int fired = 0;
    for (int tick = 0; tick < 25; ++tick) { // 0.5 s
        fired += weapon.Update(0.02f, true, Vec2{}, Vec2{1.0f, 0.0f},
                               projectiles);
    }
    CHECK(fired == 2);
}

WeaponDefinition ChargeWeapon(const std::string& kind) {
    WeaponDefinition definition = ProjectileWeapon();
    definition.damage = 1;
    definition.attack_cycle_seconds = 0.333f;
    definition.first_shot_delay_seconds = 0.033f;
    definition.special.kind = kind;
    definition.special.duration = 1.0f;
    definition.special.hold_speed = 1.0f;
    definition.special.audio_clip = "fx_gun_xl3";
    definition.special.audio_clip2 = "fx_laser_huge";
    WeaponProjectileVariant variant;
    variant.projectile.lifetime = 5.0f;
    variant.projectile.prefab_speed = 10.0f;
    variant.projectile.collider_half_extents = {0.1f, 0.1f};
    variant.projectile.collider_offset = {0.0f, 0.0f};
    variant.bullet_speed = 20.0f;
    variant.damage = 30;
    variant.through = 0;
    definition.special.variants.push_back(variant);
    return definition;
}

TEST_CASE("Gun020 charge uses base projectile before full and B1 at full" *
          doctest::test_suite("unit")) {
    WeaponDefinition definition = ChargeWeapon("charge_gun020");
    Weapon weapon;
    CHECK(weapon.Configure(definition));
    ProjectileSystem projectiles;
    for (int tick = 0; tick < 25; ++tick) { // 0.5 s
        CHECK(weapon.Update(0.02f, true, Vec2{}, Vec2{1.0f, 0.0f},
                            projectiles) == 0);
    }
    CHECK(weapon.IsCharging());
    CHECK(weapon.ChargePercent() == doctest::Approx(0.5f));
    CHECK(weapon.Update(0.02f, false, Vec2{}, Vec2{1.0f, 0.0f},
                        projectiles) == 1);
    CHECK(projectiles.Projectiles().size() == 1);
    CHECK(projectiles.Projectiles()[0].damage == 16);
    CHECK(projectiles.Projectiles()[0].sprite_index == -1);

    projectiles.Clear();
    for (int tick = 0; tick < 50; ++tick) { // full charge
        (void)weapon.Update(0.02f, true, Vec2{}, Vec2{1.0f, 0.0f},
                            projectiles);
    }
    CHECK(projectiles.Projectiles().size() == 1);
    CHECK(projectiles.Projectiles()[0].damage == 31);
}

TEST_CASE("Gun007 full charge fires the recovered multi-burst" *
          doctest::test_suite("unit")) {
    WeaponDefinition definition = ChargeWeapon("charge_gun007");
    definition.special.multi_count = 3;
    definition.special.continuous_count = 3;
    definition.special.a_interval = 0.0f;
    definition.special.angle = 15;
    Weapon weapon;
    CHECK(weapon.Configure(definition));
    ProjectileSystem projectiles;
    for (int tick = 0; tick < 50; ++tick) {
        (void)weapon.Update(0.02f, true, Vec2{}, Vec2{1.0f, 0.0f},
                            projectiles);
    }
    CHECK(projectiles.Projectiles().size() == 3);
    CHECK(projectiles.Projectiles()[0].through == 999);
    CHECK(projectiles.Projectiles()[0].damage == 31);
    CHECK(projectiles.Projectiles()[0].velocity.y < 0.0f);
    CHECK(projectiles.Projectiles()[2].velocity.y > 0.0f);
}

TEST_CASE("Gun005 bow charge interpolates damage and speed" *
          doctest::test_suite("unit")) {
    WeaponDefinition definition = ChargeWeapon("charge_bow");
    Weapon weapon;
    CHECK(weapon.Configure(definition));
    ProjectileSystem projectiles;
    for (int tick = 0; tick < 25; ++tick) {
        (void)weapon.Update(0.02f, true, Vec2{}, Vec2{1.0f, 0.0f},
                            projectiles);
    }
    CHECK(weapon.Update(0.02f, false, Vec2{}, Vec2{1.0f, 0.0f},
                        projectiles) == 1);
    CHECK(projectiles.Projectiles()[0].damage == 16);
    CHECK(projectiles.Projectiles()[0].velocity.Length() ==
          doctest::Approx(20.0f));
}

TEST_CASE("Charge weapons fire at zero charge and have no release threshold" *
          doctest::test_suite("unit")) {
    WeaponDefinition definition = ChargeWeapon("charge_bow");
    Weapon weapon;
    CHECK(weapon.Configure(definition));
    ProjectileSystem projectiles;
    CHECK(weapon.Update(0.02f, true, Vec2{}, Vec2{1.0f, 0.0f},
                        projectiles) == 0);
    CHECK(weapon.Update(0.02f, false, Vec2{}, Vec2{1.0f, 0.0f},
                        projectiles) == 1);
    CHECK(projectiles.Projectiles()[0].damage == 1);
}

TEST_CASE("Gun011 keeps normal event shots and fires the free end shot" *
          doctest::test_suite("unit")) {
    WeaponDefinition definition = ChargeWeapon("end_shot");
    definition.special.duration = 0.5f;
    definition.attack_cycle_seconds = 1.0f;
    definition.attack_event_times = {0.1f};
    Weapon weapon;
    CHECK(weapon.Configure(definition));
    ProjectileSystem projectiles;
    for (int tick = 0; tick < 30; ++tick) { // 0.6 s held
        (void)weapon.Update(0.02f, true, Vec2{}, Vec2{1.0f, 0.0f},
                            projectiles);
    }
    CHECK(projectiles.Projectiles().size() == 1);
    CHECK(weapon.Update(0.02f, false, Vec2{}, Vec2{1.0f, 0.0f},
                        projectiles) == 1);
    CHECK(projectiles.Projectiles().size() == 2);
}

TEST_CASE("BuffDeadNote kills the marked enemy after the recovered delay" *
          doctest::test_suite("unit")) {
    EnemySystem enemies = MakeEnemySystem();
    CHECK(enemies.SpawnEnemy(0, Vec2{}));
    CHECK(enemies.MarkForDeath(0, 1.0f));
    soulknight::game::combat::HealthState player;
    player.Reset(10);
    player.camp = 1;
    for (int tick = 0; tick < 25; ++tick) {
        (void)enemies.FixedUpdate(0.02f, Vec2{10.0f, 0.0f}, player);
    }
    CHECK_FALSE(enemies.Enemies()[0].health.dead);
    for (int tick = 0; tick < 30; ++tick) {
        (void)enemies.FixedUpdate(0.02f, Vec2{10.0f, 0.0f}, player);
    }
    CHECK(enemies.Enemies()[0].health.dead);
}

TEST_CASE("Camera flash applies recovered area damage inside its range" *
          doctest::test_suite("unit")) {
    EnemySystem enemies = MakeEnemySystem();
    CHECK(enemies.SpawnEnemy(0, Vec2{3.0f, 0.0f}));
    CHECK(enemies.SpawnEnemy(0, Vec2{20.0f, 0.0f}));
    CHECK(enemies.ApplyAreaDamage(Vec2{}, 13.0f, 2, 1) == 1);
    CHECK(enemies.Enemies()[0].health.hp == 8);
    CHECK(enemies.Enemies()[1].health.hp == 10);
}

TEST_CASE("Summon actors expire and damage nearby enemies" *
          doctest::test_suite("unit")) {
    EnemySystem enemies = MakeEnemySystem();
    CHECK(enemies.SpawnEnemy(0, Vec2{1.0f, 0.0f}));
    SummonSystem summons;
    WeaponSpecial special;
    special.sprite_index = 0;
    special.duration = 2.0f;
    special.hp = 8;
    special.damage = 3;
    special.speed = 6.0f;
    special.range = 2.0f;
    special.interval = 0.5f;
    CHECK(summons.Spawn(special, Vec2{}, 1, false) == 1);
    CHECK(summons.Summons().size() == 1);
    for (int tick = 0; tick < 30; ++tick) {
        (void)summons.Update(0.02f, Vec2{}, enemies, nullptr);
    }
    CHECK(enemies.Enemies()[0].health.hp < 10);
    for (int tick = 0; tick < 120; ++tick) {
        (void)summons.Update(0.02f, Vec2{}, enemies, nullptr);
    }
    CHECK_FALSE(summons.Summons()[0].active);
}

TEST_CASE("Funnel swords orbit the player without becoming generic shots" *
          doctest::test_suite("unit")) {
    SummonSystem summons;
    WeaponSpecial special;
    special.sprite_index = 0;
    special.duration = 8.0f;
    special.damage = 1;
    special.speed = 8.0f;
    special.range = 2.0f;
    special.interval = 2.0f;
    CHECK(summons.Spawn(special, Vec2{}, 2, true) == 2);
    CHECK(summons.Summons()[0].orbit);
    CHECK(summons.Summons()[0].orbit_radius > 0.0f);
    const Vec2 before = summons.Summons()[0].position;
    EnemySystem enemies = MakeEnemySystem();
    (void)summons.Update(0.1f, Vec2{5.0f, 0.0f}, enemies, nullptr);
    CHECK((summons.Summons()[0].position - before).LengthSquared() > 0.0f);
}
