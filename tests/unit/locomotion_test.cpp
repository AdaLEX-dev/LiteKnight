// Soul Knight Linux — input, collision, locomotion and camera tests
#include <doctest/doctest.h>

#include "engine/animation/AnimationClip.hpp"
#include "engine/animation/LocomotionAnimator.hpp"
#include "engine/collision/AabbCollision.hpp"
#include "engine/locomotion/PlayerLocomotion.hpp"
#include "engine/rendering/Camera2D.hpp"
#include "platform/input/InputManager.hpp"
#include "platform/input/MovementInput.hpp"

#include <cmath>
#include <string>

using soulknight::engine::Camera2D;
using soulknight::engine::Vec2;
using soulknight::engine::animation::AnimationClip;
using soulknight::engine::animation::LocomotionAnimator;
using soulknight::engine::animation::LocomotionState;
using soulknight::engine::collision::Aabb;
using soulknight::engine::collision::AabbCollisionWorld;
using soulknight::engine::locomotion::LocomotionConfig;
using soulknight::engine::locomotion::PlayerLocomotion;
using soulknight::platform::Action;
using soulknight::platform::InputManager;
using soulknight::platform::MovementInput;
using soulknight::platform::ReadDigitalMovement;

namespace {

AnimationClip MakeClip(const std::string& id, const std::string& first_sprite) {
    AnimationClip clip;
    clip.id = id;
    clip.duration = 0.5f;
    clip.sample_rate = 8.0f;
    clip.loop = true;
    soulknight::engine::animation::SpriteTrack track;
    track.target_path = "c00/img/body";
    track.property = "m_Sprite";
    track.keys = {
        {0.0f, first_sprite},
        {0.25f, first_sprite + "_next"}
    };
    clip.tracks.push_back(track);
    return clip;
}

} // namespace

TEST_CASE("Digital movement input is cardinal/diagonal/zero deterministic" *
          doctest::test_suite("unit")) {
    InputManager input;

    input.InjectKeyDown(SDL_SCANCODE_W);
    input.ProcessEvents();
    MovementInput movement = ReadDigitalMovement(input.GetSnapshot());
    CHECK(movement.IsActive());
    CHECK(movement.direction.x == doctest::Approx(0.0f));
    CHECK(movement.direction.y == doctest::Approx(1.0f));
    CHECK(movement.weight == doctest::Approx(1.0f));

    input.InjectKeyUp(SDL_SCANCODE_W);
    input.InjectKeyDown(SDL_SCANCODE_D);
    input.ProcessEvents();
    movement = ReadDigitalMovement(input.GetSnapshot());
    CHECK(movement.direction.x == doctest::Approx(1.0f));
    CHECK(movement.direction.y == doctest::Approx(0.0f));

    input.InjectKeyDown(SDL_SCANCODE_W);
    input.ProcessEvents();
    movement = ReadDigitalMovement(input.GetSnapshot());
    CHECK(movement.direction.Length() == doctest::Approx(1.0f));
    CHECK(movement.direction.x == doctest::Approx(0.70710677f));
    CHECK(movement.direction.y == doctest::Approx(0.70710677f));

    input.InjectKeyUp(SDL_SCANCODE_D);
    input.InjectKeyUp(SDL_SCANCODE_W);
    input.ProcessEvents();
    movement = ReadDigitalMovement(input.GetSnapshot());
    CHECK_FALSE(movement.IsActive());
    CHECK(movement.direction.LengthSquared() == doctest::Approx(0.0f));
    CHECK(movement.weight == doctest::Approx(0.0f));

    input.InjectKeyDown(SDL_SCANCODE_A);
    input.InjectKeyDown(SDL_SCANCODE_D);
    input.ProcessEvents();
    movement = ReadDigitalMovement(input.GetSnapshot());
    CHECK_FALSE(movement.IsActive());
}

TEST_CASE("Player locomotion uses the recovered speed equation at fixed dt" *
          doctest::test_suite("unit")) {
    AabbCollisionWorld world;
    world.SetBounds(Aabb{{0.0f, 0.0f}, {10.0f, 10.0f}});

    PlayerLocomotion player({5.0f, 5.0f}, LocomotionConfig{});
    CHECK(player.Config().speed == doctest::Approx(6.5f));
    CHECK(player.Config().speed_rate == doctest::Approx(0.5f));
    CHECK(player.Config().EffectiveSpeed() == doctest::Approx(9.75f));
    CHECK(player.Config().collider_half_extents.x == doctest::Approx(0.4f));
    CHECK(player.Config().collider_half_extents.y == doctest::Approx(0.4f));
    CHECK(player.Config().collider_offset.x == doctest::Approx(0.0f));
    CHECK(player.Config().collider_offset.y == doctest::Approx(0.5f));

    const auto collision = player.FixedUpdate({1.0f, 0.0f}, 1.0f, 0.02f, world);
    CHECK_FALSE(collision.blocked_x);
    CHECK_FALSE(collision.blocked_y);
    CHECK(player.Position().x == doctest::Approx(5.195f));
    CHECK(player.Position().y == doctest::Approx(5.0f));
    CHECK(player.Velocity().x == doctest::Approx(9.75f));
    CHECK(player.Velocity().y == doctest::Approx(0.0f));
    CHECK(player.IsMoving());

    player.FixedUpdate({0.0f, 0.0f}, 0.0f, 0.02f, world);
    CHECK(player.Position().x == doctest::Approx(5.195f));
    CHECK(player.Position().y == doctest::Approx(5.0f));
    CHECK_FALSE(player.IsMoving());
    CHECK(player.Velocity().LengthSquared() == doctest::Approx(0.0f));
}

TEST_CASE("AABB collision blocks, slides and never leaves the body penetrated" *
          doctest::test_suite("unit")) {
    AabbCollisionWorld world;
    world.SetBounds(Aabb{{0.0f, 0.0f}, {10.0f, 10.0f}});

    Vec2 resolved;
    auto result = world.Move({9.9f, 5.0f}, {0.25f, 0.25f}, {1.0f, 0.0f}, resolved);
    CHECK(result.blocked_x);
    CHECK_FALSE(result.blocked_y);
    CHECK(resolved.x == doctest::Approx(9.75f));
    CHECK(resolved.y == doctest::Approx(5.0f));

    world.AddSolid(Aabb{{4.0f, 0.0f}, {5.0f, 10.0f}});
    result = world.Move({3.5f, 5.0f}, {0.25f, 0.25f}, {2.0f, 0.0f}, resolved);
    CHECK(result.blocked_x);
    CHECK(resolved.x == doctest::Approx(3.75f));
    CHECK(resolved.y == doctest::Approx(5.0f));

    result = world.Move({3.5f, 2.0f}, {0.25f, 0.25f}, {2.0f, 1.0f}, resolved);
    CHECK(result.blocked_x);
    CHECK_FALSE(result.blocked_y);
    CHECK(resolved.x == doctest::Approx(3.75f));
    CHECK(resolved.y == doctest::Approx(3.0f));
    CHECK(resolved.x + 0.25f <= 4.0f + 1e-5f);

    result = world.Move({9.9f, 9.9f}, {0.25f, 0.25f}, {1.0f, 1.0f}, resolved);
    CHECK(result.blocked_x);
    CHECK(result.blocked_y);
    CHECK(resolved.x == doctest::Approx(9.75f));
    CHECK(resolved.y == doctest::Approx(9.75f));
}

TEST_CASE("LocomotionAnimation switches idle/run and resets deterministically" *
          doctest::test_suite("unit")) {
    const AnimationClip idle = MakeClip("c00_idle", "idle_frame");
    const AnimationClip run = MakeClip("c00_run", "run_frame");

    LocomotionAnimator animator;
    animator.Configure(&idle, &run);
    CHECK(animator.State() == LocomotionState::Idle);
    CHECK(animator.CurrentClip() == &idle);
    CHECK(animator.Evaluate("c00/img/body") != nullptr);
    CHECK(animator.Evaluate("c00/img/body")->sprite_id == "idle_frame");

    animator.Update(true, 0.02f);
    CHECK(animator.State() == LocomotionState::Run);
    CHECK(animator.CurrentClip() == &run);
    CHECK(animator.Evaluate("c00/img/body")->sprite_id == "run_frame");
    animator.Update(true, 0.25f);
    CHECK(animator.Evaluate("c00/img/body")->sprite_id == "run_frame_next");

    animator.Update(false, 0.02f);
    CHECK(animator.State() == LocomotionState::Idle);
    CHECK(animator.CurrentClip() == &idle);
    CHECK(animator.Time() == doctest::Approx(0.02f));
    CHECK(animator.LoopCount() == 0);
    CHECK(animator.Evaluate("c00/img/body")->sprite_id == "idle_frame");

    animator.Update(true, 0.02f);
    const std::string first_run = animator.Evaluate("c00/img/body")->sprite_id;
    animator.Reset();
    CHECK(animator.State() == LocomotionState::Idle);
    CHECK(animator.CurrentClip() == &idle);
    CHECK(animator.Evaluate("c00/img/body")->sprite_id == "idle_frame");
    animator.Update(true, 0.0f);
    CHECK(animator.Evaluate("c00/img/body")->sprite_id == first_run);
}

TEST_CASE("Camera2D follow is deterministic and bounded" *
          doctest::test_suite("unit")) {
    Camera2D camera;
    camera.position = {0.0f, 0.0f};
    camera.follow_smoothing = 8.0f;
    camera.Follow({10.0f, 0.0f}, 0.02f);
    CHECK(camera.position.x > 0.0f);
    CHECK(camera.position.x < 10.0f);
    CHECK(camera.position.y == doctest::Approx(0.0f));

    Camera2D repeated = camera;
    camera.Follow({10.0f, 0.0f}, 0.02f);
    repeated.Follow({10.0f, 0.0f}, 0.02f);
    CHECK(camera.position.x == doctest::Approx(repeated.position.x));
    CHECK(camera.position.y == doctest::Approx(repeated.position.y));

    camera.follow_smoothing = 0.0f;
    camera.Follow({10.0f, 4.0f}, 0.02f);
    CHECK(camera.position.x == doctest::Approx(10.0f));
    CHECK(camera.position.y == doctest::Approx(4.0f));

    const Vec2 center = camera.WorldToScreen(camera.position);
    CHECK(center.x == doctest::Approx(960.0f));
    CHECK(center.y == doctest::Approx(540.0f));
}
