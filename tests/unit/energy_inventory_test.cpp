// Soul Knight Linux — energy consumption and two-slot inventory tests
#include <doctest/doctest.h>

#include "engine/assets/SceneAsset.hpp"
#include "game/combat/ProjectileSystem.hpp"
#include "game/combat/Weapon.hpp"
#include "game/player/Energy.hpp"
#include "game/weapons/WeaponInventory.hpp"

using soulknight::engine::Vec2;
using soulknight::engine::assets::ProjectileDefinition;
using soulknight::engine::assets::WeaponDefinition;
using soulknight::game::combat::ProjectileSystem;
using soulknight::game::combat::Weapon;
using soulknight::game::player::EnergyState;
using soulknight::game::weapons::WeaponInventory;

namespace {

WeaponDefinition MakeEnergyWeapon(int consume) {
    WeaponDefinition weapon;
    weapon.id = "weapon_test_energy";
    weapon.damage = 1;
    weapon.bullet_speed = 36.0f;
    weapon.consume = consume;
    weapon.attack_cycle_seconds = 0.05f;
    weapon.first_shot_delay_seconds = 0.0f;
    weapon.projectile.id = "bullet_0";
    weapon.projectile.sprite_index = 0;
    weapon.projectile.prefab_speed = 10.0f;
    weapon.projectile.collider_half_extents = {0.1f, 0.1f};
    return weapon;
}

} // namespace

TEST_CASE("EnergyState clamps recovered maximum and initial values" *
          doctest::test_suite("unit")) {
    EnergyState energy;
    energy.Reset(180);
    CHECK(energy.max_energy == 180);
    CHECK(energy.energy == 180);

    energy.Reset(180, 300);
    CHECK(energy.energy == 180);
    energy.Reset(180, 0);
    CHECK(energy.energy == 0);
    energy.Reset(-5, 20);
    CHECK(energy.max_energy == 0);
    CHECK(energy.energy == 0);
}

TEST_CASE("EnergyState rejects unaffordable consumption without going negative" *
          doctest::test_suite("unit")) {
    EnergyState energy;
    energy.Reset(10, 4);
    CHECK(energy.CanConsume(0));
    CHECK(energy.CanConsume(4));
    CHECK_FALSE(energy.CanConsume(5));
    CHECK(energy.Consume(4));
    CHECK(energy.energy == 0);
    CHECK_FALSE(energy.Consume(1));
    CHECK(energy.energy == 0);
}

TEST_CASE("Weapon consumes serialized energy per accepted shot" *
          doctest::test_suite("unit")) {
    Weapon weapon;
    CHECK(weapon.Configure(MakeEnergyWeapon(5)));
    ProjectileSystem projectiles;
    EnergyState energy;
    energy.Reset(12, 12);
    const Vec2 owner{0.0f, 0.0f};
    const Vec2 aim{1.0f, 0.0f};

    CHECK(weapon.Update(0.001f, true, owner, aim, projectiles, &energy) == 1);
    CHECK(energy.energy == 7);
    CHECK(projectiles.ActiveCount() == 1);

    CHECK(weapon.Update(0.05f, true, owner, aim, projectiles, &energy) == 1);
    CHECK(energy.energy == 2);

    // Only 2 energy remains, so the next 5-cost attack is rejected.
    CHECK(weapon.Update(0.05f, true, owner, aim, projectiles, &energy) == 0);
    CHECK(energy.energy == 2);
    CHECK(projectiles.ActiveCount() == 2);
}

TEST_CASE("Weapon with zero consume fires without touching energy" *
          doctest::test_suite("unit")) {
    Weapon weapon;
    CHECK(weapon.Configure(MakeEnergyWeapon(0)));
    ProjectileSystem projectiles;
    EnergyState energy;
    energy.Reset(180, 0);
    CHECK(weapon.Update(0.001f, true, Vec2{}, {1.0f, 0.0f}, projectiles,
                        &energy) == 1);
    CHECK(energy.energy == 0);
}

TEST_CASE("WeaponInventory keeps independent primary and secondary slots" *
          doctest::test_suite("unit")) {
    WeaponInventory inventory;
    inventory.Configure(4);
    CHECK(inventory.Count() == 4);
    CHECK(inventory.ActiveSlot() == 0);
    CHECK(inventory.Slot(0) == 0);
    CHECK(inventory.Slot(1) == 1);

    CHECK(inventory.EquipActive(3));
    CHECK(inventory.Slot(0) == 3);
    CHECK(inventory.SwitchSlot(1));
    CHECK(inventory.ActiveIndex() == 1);
    CHECK(inventory.EquipActive(2));
    CHECK(inventory.Slot(1) == 2);
    CHECK(inventory.SwitchSlot(0));
    CHECK(inventory.ActiveIndex() == 3);
    CHECK(inventory.SwitchSlot(1));
    CHECK(inventory.ActiveIndex() == 2);

    CHECK_FALSE(inventory.EquipActive(99));
    CHECK_FALSE(inventory.SwitchSlot(2));
}

TEST_CASE("WeaponInventory single weapon config duplicates the slot safely" *
          doctest::test_suite("unit")) {
    WeaponInventory inventory;
    inventory.Configure(1);
    CHECK(inventory.Slot(0) == 0);
    CHECK(inventory.Slot(1) == 0);
    CHECK(inventory.EquipActive(0));
    CHECK(inventory.SwitchSlot(1));
    CHECK(inventory.ActiveIndex() == 0);
}

TEST_CASE("WeaponInventory rejects use before configuration" *
          doctest::test_suite("unit")) {
    WeaponInventory inventory;
    CHECK_FALSE(inventory.EquipActive(0));
    CHECK_FALSE(inventory.SwitchSlot(0));
    CHECK(inventory.Count() == 0);
}
