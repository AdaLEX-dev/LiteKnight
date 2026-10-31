// Soul Knight Linux — 1080p logical viewport, text metrics and empty equipment
#include <doctest/doctest.h>

#include "engine/animation/AnimationClip.hpp"
#include "engine/animation/LocomotionAnimator.hpp"
#include "engine/rendering/LogicalViewport.hpp"
#include "engine/rendering/TextRenderer.hpp"
#include "game/combat/Weapon.hpp"
#include "game/weapons/WeaponInventory.hpp"
#include "game/weapons/WeaponPickup.hpp"

using soulknight::engine::Vec2;
using soulknight::engine::animation::AnimationClip;
using soulknight::engine::animation::LocomotionAnimator;
using soulknight::engine::animation::LocomotionState;
using soulknight::engine::rendering::LogicalViewport;
using soulknight::engine::rendering::TextRenderer;
using soulknight::game::combat::Weapon;
using soulknight::game::weapons::WeaponInventory;
using soulknight::game::player::EnergyState;
using soulknight::game::weapons::WeaponPickupSystem;

TEST_CASE("Logical viewport keeps 16:9 and letterboxes arbitrary framebuffers" *
          doctest::test_suite("unit")) {
    LogicalViewport viewport;
    viewport.Update(1920, 1080, 1920, 1080);
    CHECK(viewport.framebuffer_scale == doctest::Approx(1.0f));
    CHECK(viewport.framebuffer_x == 0);
    CHECK(viewport.framebuffer_y == 0);
    const Vec2 center = viewport.WindowPointToLogical({960.0f, 540.0f});
    CHECK(center.x == doctest::Approx(960.0f));
    CHECK(center.y == doctest::Approx(540.0f));

    viewport.Update(1536, 790, 1536, 790);
    CHECK(viewport.framebuffer_scale == doctest::Approx(790.0f / 1080.0f));
    CHECK(viewport.framebuffer_x > 0);
    CHECK(viewport.framebuffer_y == 0);
    CHECK(viewport.framebuffer_viewport_height == 790);
    // Pillarboxed point outside the logical viewport clamps to the edge.
    const Vec2 left = viewport.WindowPointToLogical({0.0f, 395.0f});
    CHECK(left.x == doctest::Approx(0.0f));
    const Vec2 right = viewport.WindowPointToLogical({1535.0f, 395.0f});
    CHECK(right.x == doctest::Approx(1920.0f));

    viewport.Update(2560, 1080, 2560, 1080);
    CHECK(viewport.framebuffer_scale == doctest::Approx(1.0f));
    CHECK(viewport.framebuffer_x == 320);
}

TEST_CASE("Text renderer measures deterministic pixel glyph advances" *
          doctest::test_suite("unit")) {
    TextRenderer text;
    CHECK(text.BuildFont());
    CHECK_FALSE(text.HasPixelFont());
    const float width = text.TextWidth("AB", 1.0f);
    CHECK(width == doctest::Approx(12.0f));
    CHECK(text.TextWidth("A\nBB", 1.0f) == doctest::Approx(12.0f));
    CHECK(text.LineHeight(1.0f) == doctest::Approx(10.0f));
    CHECK(text.MeasureHeight("A\nB", 1.0f) == doctest::Approx(20.0f));
}

TEST_CASE("Empty equipment is a valid state and switching to it hides the weapon" *
          doctest::test_suite("unit")) {
    WeaponInventory inventory;
    inventory.Configure(4);
    CHECK_FALSE(inventory.SlotOccupied(0));
    CHECK_FALSE(inventory.SlotOccupied(1));
    CHECK(inventory.EquipActive(2));
    CHECK(inventory.SlotOccupied(0));
    CHECK(inventory.SwitchSlot(1));
    CHECK_FALSE(inventory.SlotOccupied(1));
    // Switching to an empty slot keeps both slot identities intact.
    CHECK(inventory.Slot(0) == 2);
    CHECK(inventory.ClearActive());
    CHECK_FALSE(inventory.SlotOccupied(1));
    CHECK(inventory.SwitchSlot(0));
    CHECK(inventory.SlotOccupied(0));
}

TEST_CASE("Cleared weapon is unconfigured and cannot fire" *
          doctest::test_suite("unit")) {
    Weapon weapon;
    CHECK_FALSE(weapon.IsConfigured());
    soulknight::game::combat::ProjectileSystem projectiles;
    CHECK(weapon.Update(0.02f, true, Vec2{}, {1.0f, 0.0f},
                        projectiles) == 0);
    weapon.Clear();
    CHECK_FALSE(weapon.IsConfigured());
}

TEST_CASE("Weapon pickup nearest query does not take the weapon" *
          doctest::test_suite("unit")) {
    WeaponPickupSystem pickups;
    CHECK(pickups.Spawn(2, Vec2{1.0f, 0.0f}));
    CHECK(pickups.Spawn(4, Vec2{3.0f, 0.0f}));
    const auto* nearest = pickups.Nearest(Vec2{0.0f, 0.0f}, 4.0f);
    CHECK(nearest != nullptr);
    CHECK(nearest->weapon_index == 2);
    CHECK(pickups.ActiveCount() == 2);

    std::size_t weapon = 0;
    Vec2 position{};
    CHECK(pickups.TakeNearest(Vec2{0.0f, 0.0f}, 4.0f, &weapon, &position));
    CHECK(weapon == 2);
    CHECK(position.x == doctest::Approx(1.0f));
    CHECK(pickups.ActiveCount() == 1);
}

TEST_CASE("Recovered death clip plays once and does not fall back to idle" *
          doctest::test_suite("unit")) {
    AnimationClip idle;
    idle.id = "idle";
    idle.duration = 0.5f;
    idle.loop = true;
    idle.sample_rate = 8.0f;
    AnimationClip run = idle;
    run.id = "run";
    AnimationClip dead;
    dead.id = "dead";
    dead.duration = 2.0f;
    dead.loop = false;
    dead.sample_rate = 8.0f;

    LocomotionAnimator animator;
    animator.Configure(&idle, &run, &dead);
    CHECK(animator.PlayDead());
    CHECK(animator.State() == LocomotionState::Dead);
    CHECK(animator.CurrentClip() == &dead);
    animator.Update(false, 0.1f);
    CHECK(animator.State() == LocomotionState::Dead);
    CHECK(animator.CurrentClip() == &dead);
    CHECK(animator.Time() == doctest::Approx(0.1f));
    animator.Reset();
    CHECK(animator.State() == LocomotionState::Idle);
}
