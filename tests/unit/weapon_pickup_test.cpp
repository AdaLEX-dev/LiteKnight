// Soul Knight Linux — weapon pickup foundation tests
#include <doctest/doctest.h>

#include "game/weapons/WeaponPickup.hpp"

using soulknight::engine::Vec2;
using soulknight::game::weapons::WeaponPickupSystem;

TEST_CASE("Weapon pickup takes the nearest active weapon deterministically" *
          doctest::test_suite("unit")) {
    WeaponPickupSystem pickups;
    CHECK(pickups.Spawn(1, Vec2{2.0f, 0.0f}));
    CHECK(pickups.Spawn(3, Vec2{0.5f, 0.0f}));
    CHECK(pickups.Spawn(2, Vec2{5.0f, 0.0f}));
    CHECK(pickups.ActiveCount() == 3);

    std::size_t weapon = 0;
    CHECK(pickups.TakeNearest(Vec2{0.0f, 0.0f}, 1.0f, &weapon));
    CHECK(weapon == 3);
    CHECK(pickups.ActiveCount() == 2);
    CHECK_FALSE(pickups.TakeNearest(Vec2{0.0f, 0.0f}, 1.0f, &weapon));
    CHECK(pickups.TakeNearest(Vec2{2.0f, 0.1f}, 1.0f, &weapon));
    CHECK(weapon == 1);
}

TEST_CASE("Weapon pickup animation advances only while active" *
          doctest::test_suite("unit")) {
    WeaponPickupSystem pickups;
    CHECK(pickups.Spawn(0, Vec2{}));
    pickups.Update(0.5f);
    CHECK(pickups.Pickups()[0].animation_time == doctest::Approx(0.5f));
    pickups.Update(-1.0f);
    CHECK(pickups.Pickups()[0].animation_time == doctest::Approx(0.5f));

    std::size_t weapon = 0;
    CHECK(pickups.TakeNearest(Vec2{}, 1.0f, &weapon));
    pickups.Update(2.0f);
    CHECK(pickups.Pickups()[0].animation_time == doctest::Approx(0.5f));
}

TEST_CASE("Weapon pickup radius boundary is inclusive and zero radius rejects" *
          doctest::test_suite("unit")) {
    WeaponPickupSystem pickups;
    CHECK(pickups.Spawn(0, Vec2{1.0f, 0.0f}));
    std::size_t weapon = 0;
    CHECK_FALSE(pickups.TakeNearest(Vec2{}, 0.0f, &weapon));
    CHECK(pickups.TakeNearest(Vec2{}, 1.0f, &weapon));
    CHECK(pickups.ActiveCount() == 0);
}
