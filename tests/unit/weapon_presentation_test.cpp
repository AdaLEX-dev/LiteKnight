// Soul Knight Linux — authoritative weapon presentation regression
#include <doctest/doctest.h>

#include "engine/assets/SceneAsset.hpp"
#include "game/weapons/WeaponRenderer.hpp"

#include <cmath>

using soulknight::engine::Vec2;
using soulknight::engine::assets::WeaponDefinition;
using soulknight::engine::assets::WeaponVisualKey;
using soulknight::game::weapons::WeaponRenderer;
using soulknight::game::weapons::WeaponPresentation;

namespace {

WeaponDefinition PresentationWeapon() {
    WeaponDefinition weapon;
    weapon.id = "weapon_presentation";
    weapon.visual_offset = {0.10f, 0.20f};
    weapon.visual_rotation = 0.0f;
    weapon.visual_scale = 1.0f;
    weapon.muzzle_sprite = "muzzle_test";
    weapon.muzzle_offset = {0.6f, 0.15f};
    weapon.visual_position_x = {
        WeaponVisualKey{0.0f, 0.0f}, WeaponVisualKey{0.10f, -0.20f},
        WeaponVisualKey{0.20f, 0.0f}};
    weapon.visual_rotation_z = {
        WeaponVisualKey{0.0f, 0.0f}, WeaponVisualKey{0.10f, 5.0f},
        WeaponVisualKey{0.20f, 0.0f}};
    weapon.muzzle_enabled = {
        WeaponVisualKey{0.0f, 0.0f}, WeaponVisualKey{0.05f, 1.0f},
        WeaponVisualKey{0.15f, 0.0f}};
    return weapon;
}

} // namespace

TEST_CASE("Walking never moves or animates the weapon" *
          doctest::test_suite("unit")) {
    const WeaponDefinition weapon = PresentationWeapon();
    const Vec2 hand{0.0f, 0.5f};
    const WeaponPresentation idle = WeaponRenderer::ComputePresentation(
        weapon, {0.0f, 0.0f}, {1.0f, 0.0f}, hand, 0.0f, false, 0.22f);
    const WeaponPresentation walk_a = WeaponRenderer::ComputePresentation(
        weapon, {10.0f, 10.0f}, {1.0f, 0.0f}, hand, 0.0f, false, 0.22f);
    const WeaponPresentation walk_b = WeaponRenderer::ComputePresentation(
        weapon, {11.0f, 10.0f}, {1.0f, 0.0f}, hand, 0.0f, false, 0.22f);
    CHECK(idle.position.x == doctest::Approx(0.10f));
    CHECK(idle.position.y == doctest::Approx(0.70f));
    CHECK(walk_a.position.x - 10.0f == doctest::Approx(idle.position.x));
    CHECK(walk_b.position.x - 11.0f == doctest::Approx(idle.position.x));
    CHECK(walk_a.rotation == doctest::Approx(idle.rotation));
    CHECK(walk_b.rotation == doctest::Approx(idle.rotation));
    CHECK_FALSE(walk_a.has_muzzle);
}

TEST_CASE("Aiming rotates around the fixed weapon anchor" *
          doctest::test_suite("unit")) {
    const WeaponDefinition weapon = PresentationWeapon();
    const WeaponPresentation right = WeaponRenderer::ComputePresentation(
        weapon, {2.0f, 3.0f}, {1.0f, 0.0f}, {0.0f, 0.5f}, 0.0f, false, 0.22f);
    const WeaponPresentation up = WeaponRenderer::ComputePresentation(
        weapon, {2.0f, 3.0f}, {0.0f, 1.0f}, {0.0f, 0.5f}, 0.0f, false, 0.22f);
    CHECK(up.position.x == doctest::Approx(right.position.x));
    CHECK(up.position.y == doctest::Approx(right.position.y));
    CHECK(up.rotation == doctest::Approx(right.rotation +
                                        3.14159265358979323846f / 2.0f));
}

TEST_CASE("Firing recoil is tiny and recovers; muzzle is per-weapon" *
          doctest::test_suite("unit")) {
    WeaponDefinition weapon = PresentationWeapon();
    const Vec2 hand{0.0f, 0.5f};
    const WeaponPresentation idle = WeaponRenderer::ComputePresentation(
        weapon, {0.0f, 0.0f}, {1.0f, 0.0f}, hand, 0.0f, false, 0.22f);
    // Peak recoil in the recovered curve is -0.2 at 0.05 s; subtle scale 0.22
    // leaves only 4.4 cm of transient translation.
    const WeaponPresentation fire = WeaponRenderer::ComputePresentation(
        weapon, {0.0f, 0.0f}, {1.0f, 0.0f}, hand, 0.05f, true, 0.22f);
    CHECK(std::abs(fire.position.x - idle.position.x) <= 0.05f);
    CHECK(std::abs(fire.position.y - idle.position.y) <= 0.05f);
    CHECK(std::abs(fire.rotation - idle.rotation) < 0.1f);
    CHECK(fire.has_muzzle);
    CHECK(fire.muzzle_scale == doctest::Approx(0.22f));
    const WeaponPresentation recovered = WeaponRenderer::ComputePresentation(
        weapon, {0.0f, 0.0f}, {1.0f, 0.0f}, hand, 0.05f, false, 0.22f);
    CHECK_FALSE(recovered.has_muzzle);

    weapon.muzzle_sprite.clear();
    const WeaponPresentation no_flash = WeaponRenderer::ComputePresentation(
        weapon, {0.0f, 0.0f}, {1.0f, 0.0f}, hand, 0.05f, true, 0.22f);
    CHECK_FALSE(no_flash.has_muzzle);
}
