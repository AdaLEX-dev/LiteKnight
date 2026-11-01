// Soul Knight Linux — omnidirectional movement, facing and animation tests
#include <doctest/doctest.h>

#include "engine/animation/AnimationClip.hpp"
#include "engine/animation/LocomotionAnimator.hpp"
#include "engine/assets/SceneAsset.hpp"
#include "engine/collision/AabbCollision.hpp"
#include "engine/locomotion/FacingDirection.hpp"
#include "engine/locomotion/PlayerLocomotion.hpp"
#include "engine/rendering/Camera2D.hpp"
#include "engine/rendering/PlayerRenderSync.hpp"
#include "platform/input/InputManager.hpp"
#include "platform/input/MovementInput.hpp"

#include <cmath>
#include <string>
#include <vector>

using soulknight::engine::Camera2D;
using soulknight::engine::Vec2;
using soulknight::engine::animation::AnimationClip;
using soulknight::engine::animation::LocomotionAnimator;
using soulknight::engine::animation::LocomotionState;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::assets::SpriteInstance;
using soulknight::engine::collision::AabbCollisionWorld;
using soulknight::engine::locomotion::FacingDirection;
using soulknight::engine::locomotion::PlayerLocomotion;
using soulknight::engine::locomotion::ResolveAimFacing;
using soulknight::engine::locomotion::ResolveBodyFacing;
using soulknight::engine::locomotion::ResolveFacing;
using soulknight::engine::rendering::BindPlayerRenderInstances;
using soulknight::engine::rendering::PlayerRenderBinding;
using soulknight::engine::rendering::ResolvePlayerBodyFlipX;
using soulknight::engine::rendering::ResolvePlayerRenderPosition;
using soulknight::platform::InputManager;
using soulknight::platform::MovementInput;
using soulknight::platform::ReadDigitalMovement;

namespace {

AnimationClip MakeClip(const std::string& id) {
    AnimationClip clip;
    clip.id = id;
    clip.duration = 0.5f;
    clip.sample_rate = 8.0f;
    clip.loop = true;
    soulknight::engine::animation::SpriteTrack track;
    track.target_path = "c00/img/body";
    track.property = "m_Sprite";
    track.keys = {
        {0.0f, id + "_frame0"},
        {0.25f, id + "_frame1"},
        {0.5f, id + "_frame0"},
    };
    clip.tracks.push_back(track);
    return clip;
}

SceneAsset MakePlayerScene() {
    SceneAsset scene;
    SpriteInstance body;
    body.path = "c00/img/body";
    body.position = {20.0f, 20.0f};
    scene.instances.push_back(body);
    SpriteInstance shadow;
    shadow.path = "c00/shadow";
    shadow.position = {20.0f, 19.95f};
    scene.instances.push_back(shadow);
    return scene;
}

MovementInput Digital(const InputManager& input) {
    return ReadDigitalMovement(input.GetSnapshot());
}

} // namespace

TEST_CASE("Eight-way digital input is normalized and cancels opposing axes" *
          doctest::test_suite("unit")) {
    struct InputCase {
        std::vector<SDL_Scancode> keys;
        Vec2 expected;
    };
    const std::vector<InputCase> cases = {
        {{SDL_SCANCODE_W}, {0.0f, 1.0f}},
        {{SDL_SCANCODE_S}, {0.0f, -1.0f}},
        {{SDL_SCANCODE_A}, {-1.0f, 0.0f}},
        {{SDL_SCANCODE_D}, {1.0f, 0.0f}},
        {{SDL_SCANCODE_W, SDL_SCANCODE_A}, {-0.70710678f, 0.70710678f}},
        {{SDL_SCANCODE_W, SDL_SCANCODE_D}, {0.70710678f, 0.70710678f}},
        {{SDL_SCANCODE_S, SDL_SCANCODE_A}, {-0.70710678f, -0.70710678f}},
        {{SDL_SCANCODE_S, SDL_SCANCODE_D}, {0.70710678f, -0.70710678f}},
    };
    for (const InputCase& test : cases) {
        InputManager input;
        for (SDL_Scancode key : test.keys) input.InjectKeyDown(key);
        input.ProcessEvents();
        const MovementInput movement = Digital(input);
        CHECK(movement.IsActive());
        CHECK(movement.weight == doctest::Approx(1.0f));
        CHECK(movement.direction.x == doctest::Approx(test.expected.x));
        CHECK(movement.direction.y == doctest::Approx(test.expected.y));
        CHECK(movement.direction.Length() == doctest::Approx(1.0f));
    }

    {
        InputManager input;
        input.InjectKeyDown(SDL_SCANCODE_W);
        input.InjectKeyDown(SDL_SCANCODE_S);
        input.ProcessEvents();
        CHECK_FALSE(Digital(input).IsActive());
    }
    {
        InputManager input;
        input.InjectKeyDown(SDL_SCANCODE_A);
        input.InjectKeyDown(SDL_SCANCODE_D);
        input.ProcessEvents();
        CHECK_FALSE(Digital(input).IsActive());
    }
}

TEST_CASE("Cardinal and diagonal movement preserve the recovered 9.75 speed" *
          doctest::test_suite("unit")) {
    AabbCollisionWorld world;
    world.SetBounds({{0.0f, 0.0f}, {41.0f, 41.0f}});
    const Vec2 directions[] = {
        {0.0f, 1.0f}, {0.0f, -1.0f}, {-1.0f, 0.0f}, {1.0f, 0.0f},
        {-0.70710678f, 0.70710678f}, {0.70710678f, 0.70710678f},
        {-0.70710678f, -0.70710678f}, {0.70710678f, -0.70710678f},
    };
    for (const Vec2& direction : directions) {
        PlayerLocomotion player({20.0f, 20.0f});
        player.FixedUpdate(direction, 1.0f, 0.02f, world);
        CHECK(player.Velocity().Length() == doctest::Approx(9.75f));
        const Vec2 displacement = player.Position() - Vec2{20.0f, 20.0f};
        CHECK(displacement.Length() == doctest::Approx(0.195f));
        CHECK(player.Position().Distance({20.0f, 20.0f}) ==
              doctest::Approx(0.195f));
    }
}

TEST_CASE("Facing is horizontal-only and vertical input retains the last facing" *
          doctest::test_suite("unit")) {
    FacingDirection facing = FacingDirection::Right;
    facing = ResolveFacing(facing, {0.0f, 1.0f});
    CHECK(facing == FacingDirection::Right);
    facing = ResolveFacing(facing, {0.0f, -1.0f});
    CHECK(facing == FacingDirection::Right);
    facing = ResolveFacing(facing, {-1.0f, 0.0f});
    CHECK(facing == FacingDirection::Left);
    facing = ResolveFacing(facing, {0.0f, 1.0f});
    CHECK(facing == FacingDirection::Left);
    facing = ResolveFacing(facing, {-0.70710678f, 0.70710678f});
    CHECK(facing == FacingDirection::Left);
    facing = ResolveFacing(facing, {0.70710678f, 0.70710678f});
    CHECK(facing == FacingDirection::Right);
    facing = ResolveFacing(facing, {-0.70710678f, -0.70710678f});
    CHECK(facing == FacingDirection::Left);
    facing = ResolveFacing(facing, {0.70710678f, -0.70710678f});
    CHECK(facing == FacingDirection::Right);

    CHECK_FALSE(ResolvePlayerBodyFlipX(false, FacingDirection::Right));
    CHECK(ResolvePlayerBodyFlipX(false, FacingDirection::Left));
    CHECK(ResolvePlayerBodyFlipX(true, FacingDirection::Right));
    CHECK_FALSE(ResolvePlayerBodyFlipX(true, FacingDirection::Left));
}

TEST_CASE("Body facing follows movement and only uses aim while idle" *
          doctest::test_suite("unit")) {
    FacingDirection facing = FacingDirection::Right;
    // Moving left wins over an aim vector pointing right.
    facing = ResolveBodyFacing(facing, {-1.0f, 0.0f}, {1.0f, 0.0f}, true);
    CHECK(facing == FacingDirection::Left);
    // Vertical movement retains the last horizontal body facing.
    facing = ResolveBodyFacing(facing, {0.0f, 1.0f}, {1.0f, 0.0f}, true);
    CHECK(facing == FacingDirection::Left);
    // Idle body follows the aim direction.
    facing = ResolveBodyFacing(facing, {0.0f, 0.0f}, {1.0f, 0.0f}, false);
    CHECK(facing == FacingDirection::Right);
    CHECK(ResolveAimFacing(FacingDirection::Right, {0.0f, 1.0f}) ==
          FacingDirection::Right);
}

TEST_CASE("Run animation and player render stay synchronized for every movement direction" *
          doctest::test_suite("unit")) {
    const SceneAsset scene = MakePlayerScene();
    const PlayerRenderBinding binding = BindPlayerRenderInstances(scene);
    CHECK(binding.IsValid());
    const AnimationClip idle = MakeClip("idle");
    const AnimationClip run = MakeClip("run");
    LocomotionAnimator animator;
    animator.Configure(&idle, &run);
    Camera2D camera;
    camera.position = {20.0f, 20.0f};

    const Vec2 directions[] = {
        {0.0f, 1.0f}, {0.0f, -1.0f}, {-1.0f, 0.0f}, {1.0f, 0.0f},
        {-0.70710678f, 0.70710678f}, {0.70710678f, 0.70710678f},
        {-0.70710678f, -0.70710678f}, {0.70710678f, -0.70710678f},
    };
    AabbCollisionWorld world;
    world.SetBounds({{0.0f, 0.0f}, {41.0f, 41.0f}});
    PlayerLocomotion player({20.0f, 20.0f});
    for (const Vec2& direction : directions) {
        const auto collision = player.FixedUpdate(direction, 1.0f, 0.02f, world);
        CHECK_FALSE(collision.blocked_x);
        CHECK_FALSE(collision.blocked_y);
        animator.Update(true, 1.0f / 50.0f);
        camera.Follow(player.Position(), 1.0f / 50.0f);
        CHECK(animator.State() == LocomotionState::Run);

        const Vec2 body = ResolvePlayerRenderPosition(
            scene, binding, binding.body_instance, player.Position());
        const Vec2 shadow = ResolvePlayerRenderPosition(
            scene, binding, binding.shadow_instance, player.Position());
        CHECK(body.x == doctest::Approx(player.Position().x));
        CHECK(body.y == doctest::Approx(player.Position().y));
        CHECK(shadow.x == doctest::Approx(player.Position().x));
        CHECK(shadow.y == doctest::Approx(player.Position().y - 0.05f));
        CHECK(animator.Evaluate("c00/img/body") != nullptr);
    }

    animator.Update(false, 1.0f / 50.0f);
    CHECK(animator.State() == LocomotionState::Idle);
    const Vec2 body = ResolvePlayerRenderPosition(
        scene, binding, binding.body_instance, player.Position());
    CHECK(body.x == doctest::Approx(player.Position().x));
    CHECK(body.y == doctest::Approx(player.Position().y));
}
