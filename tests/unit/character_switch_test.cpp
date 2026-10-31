// Soul Knight Linux — dev character switching tests
#include <doctest/doctest.h>

#include "engine/animation/AnimationClip.hpp"
#include "engine/assets/SceneAsset.hpp"
#include "engine/locomotion/PlayerLocomotion.hpp"
#include "engine/rendering/Camera2D.hpp"
#include "engine/rendering/PlayerRenderSync.hpp"
#include "game/player/PlayerCharacterSet.hpp"

#include <string>

using soulknight::engine::Camera2D;
using soulknight::engine::Vec2;
using soulknight::engine::animation::AnimationClip;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::assets::SceneCharacter;
using soulknight::engine::assets::SpriteData;
using soulknight::engine::assets::SpriteInstance;
using soulknight::engine::locomotion::PlayerLocomotion;
using soulknight::engine::rendering::BindPlayerRenderInstances;
using soulknight::engine::rendering::PlayerRenderBinding;
using soulknight::engine::rendering::ResolvePlayerRenderPosition;
using soulknight::game::player::PlayerCharacterSet;

namespace {

AnimationClip MakeClip(const std::string& id, const std::string& sprite_id) {
    AnimationClip clip;
    clip.id = id;
    clip.duration = 0.5f;
    clip.sample_rate = 8.0f;
    clip.loop = true;
    soulknight::engine::animation::SpriteTrack track;
    track.target_path = "c00/img/body";
    track.property = "m_Sprite";
    track.keys = {{0.0f, sprite_id}, {0.25f, sprite_id}};
    clip.tracks.push_back(track);
    return clip;
}

SpriteData MakeSprite(const std::string& id) {
    SpriteData sprite;
    sprite.id = id;
    sprite.texture_id = 0;
    sprite.texture_width = 16;
    sprite.texture_height = 16;
    sprite.rect = {0.0f, 0.0f, 16.0f, 16.0f};
    sprite.pivot = {0.5f, 0.0f};
    sprite.pixels_per_unit = 16.0f;
    return sprite;
}

SceneAsset MakeCharacterScene() {
    SceneAsset scene;
    scene.sprites.push_back(MakeSprite("knight_frame"));
    scene.sprites.push_back(MakeSprite("other_frame"));
    scene.animations.push_back(MakeClip("c00_idle", "knight_frame"));
    scene.animations.push_back(MakeClip("c00_run", "knight_frame"));
    scene.animations.push_back(MakeClip("c01_idle", "other_frame"));
    scene.animations.push_back(MakeClip("c01_run", "other_frame"));

    SceneCharacter knight;
    knight.id = "c00";
    knight.controller_name = "char_0_skin_0";
    knight.body_sprite_id = "knight_frame";
    knight.body_sprite_index = 0;
    knight.idle_animation_id = "c00_idle";
    knight.run_animation_id = "c00_run";
    scene.characters.push_back(knight);

    SceneCharacter other;
    other.id = "c01";
    other.controller_name = "char_1_skin_0";
    other.body_sprite_id = "other_frame";
    other.body_sprite_index = 1;
    other.idle_animation_id = "c01_idle";
    other.run_animation_id = "c01_run";
    scene.characters.push_back(other);

    SpriteInstance body;
    body.path = "c00/img/body";
    body.position = {20.0f, 20.0f};
    body.sprite_index = 0;
    scene.instances.push_back(body);
    SpriteInstance shadow;
    shadow.path = "c00/shadow";
    shadow.position = {20.0f, 19.95f};
    scene.instances.push_back(shadow);
    return scene;
}

} // namespace

TEST_CASE("Recovered character switching changes sprite and clip source only" *
          doctest::test_suite("unit")) {
    SceneAsset scene = MakeCharacterScene();
    PlayerCharacterSet characters;
    std::string error;
    CHECK(characters.Configure(scene, &error));
    CHECK(error.empty());
    CHECK(characters.Count() == 2);
    CHECK(characters.Active().id == "c00");
    CHECK(characters.IdleClip() == &scene.animations[0]);
    CHECK(characters.RunClip() == &scene.animations[1]);
    CHECK(characters.Active().body_sprite_index == 0);

    const PlayerRenderBinding binding = BindPlayerRenderInstances(scene);
    CHECK(binding.IsValid());
    PlayerLocomotion player({20.0f, 25.0f});
    Camera2D camera;
    camera.position = {20.0f, 25.0f};
    const Vec2 before = player.Position();

    CHECK(characters.Select(1));
    CHECK(characters.Active().id == "c01");
    CHECK(characters.Active().body_sprite_index == 1);
    CHECK(characters.IdleClip() == &scene.animations[2]);
    CHECK(characters.RunClip() == &scene.animations[3]);
    CHECK(player.Position() == before);
    CHECK(camera.position == Vec2{20.0f, 25.0f});
    CHECK(ResolvePlayerRenderPosition(
              scene, binding, binding.body_instance, player.Position()) == before);
    CHECK(ResolvePlayerRenderPosition(
              scene, binding, binding.shadow_instance, player.Position()) ==
          Vec2{before.x, before.y - 0.05f});

    CHECK_FALSE(characters.Select(2));
    CHECK(characters.Active().id == "c01");
    characters.Cycle(1);
    CHECK(characters.Active().id == "c00");
    characters.Cycle(-1);
    CHECK(characters.Active().id == "c01");

    // Movement and camera continue through the switch without changing the
    // render binding or shadow offset.
    player.FixedUpdate({1.0f, 0.0f}, 1.0f, 0.02f,
                       soulknight::engine::collision::AabbCollisionWorld{});
    camera.Follow(player.Position(), 0.02f);
    CHECK(player.Position().x > before.x);
    CHECK(camera.position.x > before.x);
    CHECK(ResolvePlayerRenderPosition(
              scene, binding, binding.body_instance, player.Position()) ==
          player.Position());
    CHECK(ResolvePlayerRenderPosition(
              scene, binding, binding.shadow_instance, player.Position()) ==
          Vec2{player.Position().x, player.Position().y - 0.05f});
}
