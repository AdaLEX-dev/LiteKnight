// Soul Knight Linux — sprite animation and clip tests
#include <doctest/doctest.h>

#include "engine/animation/AnimationClip.hpp"
#include "engine/animation/AnimationClipLoader.hpp"
#include "engine/animation/AnimationPlayer.hpp"
#include "engine/assets/SceneLoader.hpp"
#include "platform/filesystem/FileSystem.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

#include <cmath>
#include <string>
#include <vector>

using soulknight::engine::animation::AnimationClip;
using soulknight::engine::animation::AnimationClipLoader;
using soulknight::engine::animation::AnimationPlayer;
using soulknight::engine::animation::AnimationSource;
using soulknight::engine::animation::AnimationUpdateMode;
using soulknight::engine::animation::SpriteKeyframe;
using soulknight::engine::animation::SpriteTrack;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::assets::SceneLoader;
using soulknight::platform::FileSystem;

namespace {

AnimationClip MakeLoopClip() {
    AnimationClip clip;
    clip.id = "test_loop";
    clip.duration = 0.5f;
    clip.sample_rate = 8.0f;
    clip.loop = true;
    clip.update_mode = AnimationUpdateMode::Normal;

    SpriteTrack track;
    track.target_path = "c00/img/body";
    track.property = "m_Sprite";
    track.keys = {
        {0.0f, "frame0"},
        {0.125f, "frame1"},
        {0.25f, "frame2"},
        {0.375f, "frame3"}
    };
    clip.tracks.push_back(track);
    return clip;
}

const std::string* EvaluateSprite(const AnimationPlayer& player,
                                  const std::string& target) {
    const SpriteKeyframe* key = player.Evaluate(target);
    return key != nullptr ? &key->sprite_id : nullptr;
}

} // namespace

TEST_CASE("AnimationClipLoader parses native clip metadata" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());

    AnimationClip clip;
    std::string error;
    CHECK(AnimationClipLoader::Load(filesystem, "test/animations/test_clip.json",
                                    clip, &error));
    CHECK(error.empty());
    CHECK(clip.id == "test_walk");
    CHECK(clip.duration == doctest::Approx(0.5f));
    CHECK(clip.sample_rate == doctest::Approx(4.0f));
    CHECK(clip.loop);
    CHECK(clip.update_mode == AnimationUpdateMode::Normal);
    CHECK(clip.tracks.size() == 1);
    CHECK(clip.tracks[0].target_path == "c00/img/body");
    CHECK(clip.tracks[0].property == "m_Sprite");
    CHECK(clip.tracks[0].keys.size() == 2);
    CHECK(clip.tracks[0].keys[0].sprite_id == "frame0");
    CHECK(clip.tracks[0].keys[1].time == doctest::Approx(0.25f));

    AnimationClip malformed;
    std::string malformed_error;
    CHECK_FALSE(AnimationClipLoader::Parse("{", malformed, &malformed_error));
    CHECK_FALSE(malformed_error.empty());

    AnimationClip missing_key;
    std::string missing_error;
    CHECK_FALSE(AnimationClipLoader::Parse(
        R"({"id":"bad","duration":1.0,"sample_rate":8.0,"loop":true,"tracks":[{"target_path":"x","keys":[{"time":0.0}]}]})",
        missing_key, &missing_error));
    CHECK_FALSE(missing_error.empty());
}

TEST_CASE("SpriteTrack uses discrete keys at exact boundaries" *
          doctest::test_suite("unit")) {
    SpriteTrack track;
    track.target_path = "body";
    track.property = "m_Sprite";
    track.keys = {
        {0.0f, "a"},
        {0.25f, "b"},
        {0.5f, "c"},
        {0.5f, "d"}
    };

    CHECK(track.IsValid());
    CHECK(track.Evaluate(0.0f)->sprite_id == "a");
    CHECK(track.Evaluate(0.2499f)->sprite_id == "a");
    CHECK(track.Evaluate(0.25f)->sprite_id == "b");
    CHECK(track.Evaluate(0.4999f)->sprite_id == "b");
    CHECK(track.Evaluate(0.5f)->sprite_id == "d"); // last duplicate wins
    CHECK(track.Evaluate(1.0f)->sprite_id == "d");

    SpriteTrack only_key;
    only_key.target_path = "body";
    only_key.keys = {{0.25f, "only"}};
    CHECK(only_key.Evaluate(0.0f)->sprite_id == "only");
    CHECK(only_key.Evaluate(10.0f)->sprite_id == "only");

    SpriteTrack empty;
    empty.target_path = "body";
    CHECK(empty.Evaluate(0.0f) == nullptr);
    CHECK(empty.IsValid()); // empty key list is structurally valid
}

TEST_CASE("AnimationPlayer loops deterministically and clamps non-loop clips" *
          doctest::test_suite("unit")) {
    const AnimationClip clip = MakeLoopClip();
    AnimationPlayer player;
    player.Play(clip);

    CHECK(player.IsPlaying());
    CHECK(player.Time() == doctest::Approx(0.0f));
    CHECK(player.NormalizedTime() == doctest::Approx(0.0f));
    CHECK(EvaluateSprite(player, "c00/img/body") != nullptr);
    CHECK(*EvaluateSprite(player, "c00/img/body") == "frame0");

    player.Update(0.125f);
    CHECK(player.Time() == doctest::Approx(0.125f));
    CHECK(*EvaluateSprite(player, "c00/img/body") == "frame1");
    player.Update(0.125f);
    CHECK(*EvaluateSprite(player, "c00/img/body") == "frame2");
    CHECK(*EvaluateSprite(player, "c00/img/body") ==
          *EvaluateSprite(player, "c00/img/body"));
    player.Update(0.1249f);
    CHECK(player.Time() == doctest::Approx(0.3749f));
    CHECK(*EvaluateSprite(player, "c00/img/body") == "frame2");

    player.Update(0.0001f);
    CHECK(player.Time() == doctest::Approx(0.375f));
    CHECK(*EvaluateSprite(player, "c00/img/body") == "frame3");
    player.Update(0.125f);
    CHECK(player.LoopCount() == 1);
    CHECK(player.Time() == doctest::Approx(0.0f));
    CHECK(*EvaluateSprite(player, "c00/img/body") == "frame0");

    player.Update(0.5f);
    CHECK(player.LoopCount() == 2);
    CHECK(player.Time() == doctest::Approx(0.0f));
    player.Update(-1.0f);
    CHECK(player.Time() == doctest::Approx(0.0f));

    AnimationClip once = MakeLoopClip();
    once.loop = false;
    AnimationPlayer once_player;
    once_player.Play(once);
    once_player.Update(2.0f);
    CHECK_FALSE(once_player.IsPlaying());
    CHECK(once_player.Time() == doctest::Approx(0.5f));
    CHECK(once_player.NormalizedTime() == doctest::Approx(1.0f));
    CHECK(*EvaluateSprite(once_player, "c00/img/body") == "frame3");

    once_player.Reset();
    CHECK(once_player.IsPlaying());
    CHECK(once_player.Time() == doctest::Approx(0.0f));
    CHECK(once_player.LoopCount() == 0);
    CHECK(*EvaluateSprite(once_player, "c00/img/body") == "frame0");

    const AnimationClip empty;
    AnimationPlayer stopped;
    stopped.Play(empty);
    stopped.Update(1.0f);
    CHECK_FALSE(stopped.IsPlaying());
    CHECK(stopped.Clip() == &empty);
}

TEST_CASE("SceneLoader loads animation references and rejects unknown targets" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());

    SceneAsset scene;
    std::string error;
    CHECK(SceneLoader::Load(filesystem, "test/scene_animation_test.json",
                            scene, &error));
    CHECK(error.empty());
    CHECK(scene.animations.size() == 1);
    const AnimationClip& clip = scene.animations[0];
    CHECK(clip.id == "test_walk");
    CHECK(clip.duration == doctest::Approx(0.5f));
    CHECK(clip.tracks.size() == 1);
    CHECK(clip.tracks[0].target_path == "c00/img/body");
    CHECK(clip.tracks[0].keys.size() == 2);
    CHECK(scene.FindSpriteIndex("frame0") == 0);
    CHECK(scene.FindSpriteIndex("frame1") == 1);
    CHECK(scene.FindSpriteIndex("missing") == -1);

    SceneAsset bad_scene;
    std::string bad_error;
    CHECK_FALSE(SceneLoader::Load(filesystem, "test/scene_animation_bad_test.json",
                                  bad_scene, &bad_error));
    CHECK(bad_error.find("unknown sprite") != std::string::npos);
}

TEST_CASE("Real c00 idle metadata resolves against the r1_1 scene" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());

    const auto scene_text = filesystem.ReadTextFile("test/scenes/r1_1_visual.json");
    const auto animation_text =
        filesystem.ReadTextFile("test/scenes/animations/c00_idle.json");
    CHECK(scene_text.has_value());
    CHECK(animation_text.has_value());

    const nlohmann::json scene = nlohmann::json::parse(*scene_text, nullptr, false);
    CHECK_FALSE(scene.is_discarded());
    CHECK(scene.contains("textures"));
    CHECK(scene.contains("sprites"));

    AnimationClip clip;
    std::string error;
    CHECK(AnimationClipLoader::Parse(*animation_text, clip, &error));
    CHECK(clip.id == "c00_idle");
    CHECK(clip.duration == doctest::Approx(0.5f));
    CHECK(clip.sample_rate == doctest::Approx(8.0f));
    CHECK(clip.loop);
    CHECK(clip.tracks.size() == 1);
    CHECK(clip.tracks[0].target_path == "c00/img/body");
    CHECK(clip.tracks[0].keys.size() == 4);
    CHECK(clip.tracks[0].keys[0].time == doctest::Approx(0.0f));
    CHECK(clip.tracks[0].keys[1].time == doctest::Approx(0.125f));
    CHECK(clip.tracks[0].keys[2].time == doctest::Approx(0.25f));
    CHECK(clip.tracks[0].keys[3].time == doctest::Approx(0.375f));

    for (const SpriteKeyframe& key : clip.tracks[0].keys) {
        bool found = false;
        for (const auto& sprite : scene["sprites"]) {
            if (sprite.value("id", "") == key.sprite_id) {
                found = true;
                const auto& rect = sprite["rect"];
                const auto& textures = scene["textures"];
                bool texture_found = false;
                for (const auto& texture : textures) {
                    if (texture.value("id", "") == sprite.value("texture", "")) {
                        const auto width = texture.value("width", 0);
                        const auto height = texture.value("height", 0);
                        CHECK(rect.at(0).get<float>() >= 0.0f);
                        CHECK(rect.at(1).get<float>() >= 0.0f);
                        CHECK(rect.at(0).get<float>() + rect.at(2).get<float>() <=
                              static_cast<float>(width));
                        CHECK(rect.at(1).get<float>() + rect.at(3).get<float>() <=
                              static_cast<float>(height));
                        texture_found = true;
                        break;
                    }
                }
                CHECK(texture_found);
                break;
            }
        }
        CHECK(found);
    }

    AnimationPlayer player;
    player.Play(clip);
    player.Update(0.125f);
    CHECK(player.Evaluate("c00/img/body") != nullptr);
    CHECK(player.Evaluate("c00/img/body")->sprite_id ==
          clip.tracks[0].keys[1].sprite_id);
}

TEST_CASE("AnimationSource preserves conversion provenance fields" *
          doctest::test_suite("unit")) {
    AnimationClip clip;
    clip.id = "source_test";
    clip.duration = 1.0f;
    clip.sample_rate = 8.0f;
    AnimationSource source;
    source.clip_path_id = "123";
    source.clip_name = "clippy";
    source.binding_path = "img/body";
    source.pptr_curve_index = "3";
    clip.source = source;
    CHECK(clip.source.clip_path_id == "123");
    CHECK(clip.source.binding_path == "img/body");
    CHECK(clip.source.pptr_curve_index == "3");
}
