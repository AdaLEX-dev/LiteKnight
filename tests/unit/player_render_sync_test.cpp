// Soul Knight Linux — player simulation -> render synchronization tests
#include <doctest/doctest.h>

#include "engine/animation/AnimationClip.hpp"
#include "engine/animation/LocomotionAnimator.hpp"
#include "engine/assets/SceneAsset.hpp"
#include "engine/rendering/Camera2D.hpp"
#include "engine/rendering/PlayerRenderSync.hpp"

using soulknight::engine::Camera2D;
using soulknight::engine::Vec2;
using soulknight::engine::animation::AnimationClip;
using soulknight::engine::animation::LocomotionAnimator;
using soulknight::engine::animation::SpriteKeyframe;
using soulknight::engine::animation::SpriteTrack;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::assets::SpriteInstance;
using soulknight::engine::rendering::BindPlayerRenderInstances;
using soulknight::engine::rendering::PlayerRenderBinding;
using soulknight::engine::rendering::ResolvePlayerRenderPosition;

namespace {

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

    SpriteInstance wall;
    wall.path = "native_wall/wall601@20,13";
    wall.position = {20.0f, 13.0f};
    scene.instances.push_back(wall);
    return scene;
}

AnimationClip MakeClip(const std::string& id, const std::string& first) {
    AnimationClip clip;
    clip.id = id;
    clip.duration = 0.5f;
    clip.sample_rate = 8.0f;
    clip.loop = true;
    SpriteTrack track;
    track.target_path = "c00/img/body";
    track.property = "m_Sprite";
    track.keys = {
        SpriteKeyframe{0.0f, first},
        SpriteKeyframe{0.25f, first + "_next"},
        SpriteKeyframe{0.5f, first},
    };
    clip.tracks.push_back(track);
    return clip;
}

} // namespace

TEST_CASE("Player body and shadow bind to the recovered scene instances" *
          doctest::test_suite("unit")) {
    const SceneAsset scene = MakePlayerScene();
    const PlayerRenderBinding binding = BindPlayerRenderInstances(scene);
    CHECK(binding.IsValid());
    CHECK(binding.body_instance == 0);
    CHECK(binding.shadow_instance == 1);
    CHECK(binding.shadow_offset.x == doctest::Approx(0.0f));
    CHECK(binding.shadow_offset.y == doctest::Approx(-0.05f));

    const Vec2 player{21.0f, 20.0f};
    const Vec2 body = ResolvePlayerRenderPosition(
        scene, binding, binding.body_instance, player);
    const Vec2 shadow = ResolvePlayerRenderPosition(
        scene, binding, binding.shadow_instance, player);
    CHECK(body.x == doctest::Approx(player.x));
    CHECK(body.y == doctest::Approx(player.y));
    CHECK(shadow.x == doctest::Approx(player.x));
    CHECK(shadow.y == doctest::Approx(player.y - 0.05f));

    // Authored, non-player instances must not be dragged with the player.
    const Vec2 wall = ResolvePlayerRenderPosition(scene, binding, 2, player);
    CHECK(wall.x == doctest::Approx(20.0f));
    CHECK(wall.y == doctest::Approx(13.0f));
}

TEST_CASE("Animation frames and camera changes never move the player render transform" *
          doctest::test_suite("unit")) {
    const SceneAsset scene = MakePlayerScene();
    const PlayerRenderBinding binding = BindPlayerRenderInstances(scene);
    CHECK(binding.IsValid());

    const AnimationClip idle = MakeClip("idle", "c01_idle");
    const AnimationClip run = MakeClip("run", "c01_run");
    LocomotionAnimator animator;
    animator.Configure(&idle, &run);

    const Vec2 player{20.25f, 15.75f};
    const Vec2 expected_body = player;
    const Vec2 expected_shadow{player.x, player.y - 0.05f};

    Camera2D camera;
    camera.position = {100.0f, -50.0f};
    camera.viewport_width = 1920.0f;
    camera.viewport_height = 1080.0f;

    for (int frame = 0; frame < 24; ++frame) {
        animator.Update(frame % 2 == 0, 1.0f / 60.0f);
        camera.Follow(player, 1.0f / 60.0f);
        camera.viewport_width = 800.0f + static_cast<float>(frame) * 16.0f;
        camera.viewport_height = 600.0f + static_cast<float>(frame) * 9.0f;

        const Vec2 body = ResolvePlayerRenderPosition(
            scene, binding, binding.body_instance, player);
        const Vec2 shadow = ResolvePlayerRenderPosition(
            scene, binding, binding.shadow_instance, player);
        CHECK(body.x == doctest::Approx(expected_body.x));
        CHECK(body.y == doctest::Approx(expected_body.y));
        CHECK(shadow.x == doctest::Approx(expected_shadow.x));
        CHECK(shadow.y == doctest::Approx(expected_shadow.y));

        // The animation changes the active sprite key, not the world position.
        const SpriteKeyframe* key = animator.Evaluate("c00/img/body");
        CHECK(key != nullptr);
    }

    // Simulated movement propagates to both render instances with the shadow
    // offset preserved.
    const Vec2 moved{22.5f, 16.25f};
    const Vec2 moved_body = ResolvePlayerRenderPosition(
        scene, binding, binding.body_instance, moved);
    const Vec2 moved_shadow = ResolvePlayerRenderPosition(
        scene, binding, binding.shadow_instance, moved);
    CHECK(moved_body.x == doctest::Approx(moved.x));
    CHECK(moved_body.y == doctest::Approx(moved.y));
    CHECK(moved_shadow.y == doctest::Approx(moved.y - 0.05f));

    // Camera viewport/minimize/restore state cannot affect the result.
    camera.viewport_width = 0.0f;
    camera.viewport_height = 0.0f;
    camera.position = {0.0f, 0.0f};
    const Vec2 after_resize = ResolvePlayerRenderPosition(
        scene, binding, binding.body_instance, moved);
    CHECK(after_resize.x == doctest::Approx(moved.x));
    CHECK(after_resize.y == doctest::Approx(moved.y));
}
