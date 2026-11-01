// Soul Knight Linux — recovered enemy damage-value regression
#include <doctest/doctest.h>

#include "engine/assets/SceneLoader.hpp"
#include "game/enemy/EnemySystem.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <algorithm>
#include <string>

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

using soulknight::engine::assets::SceneAsset;
using soulknight::engine::assets::SceneLoader;
using soulknight::game::enemy::EnemySystem;
using soulknight::platform::FileSystem;
using Json = nlohmann::json;

namespace {

Json LoadDamageReference() {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    const auto text =
        filesystem.ReadTextFile("test/scenes/enemy_damage_reference.json");
    CHECK(text.has_value());
    Json reference = Json::parse(text.value_or("{}"), nullptr, false);
    CHECK_FALSE(reference.is_discarded());
    return reference;
}

} // namespace

TEST_CASE("Runtime enemy damage matches the recovered APK reference" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    SceneAsset scene;
    std::string error;
    CHECK(SceneLoader::Load(filesystem, "scenes/r1_1_visual/scene.json",
                              scene, &error));
    const Json reference = LoadDamageReference();
    CHECK(reference.contains("entries"));
    CHECK(reference["entries"].is_array());
    CHECK(reference.value("enemy_count", 0) ==
          static_cast<int>(scene.enemy_definitions.size()));

    EnemySystem enemies;
    CHECK(enemies.Configure(scene, &error));
    CHECK(enemies.DefinitionCount() == scene.enemy_definitions.size());

    int compared = 0;
    for (const Json& expected : reference["entries"]) {
        const std::string id = expected.value("id", "");
        CHECK_FALSE(id.empty());
        const auto found = std::find_if(
            enemies.Definitions().begin(), enemies.Definitions().end(),
            [&](const auto& definition) { return definition.id == id; });
        CAPTURE(id);
        CHECK(found != enemies.Definitions().end());
        CHECK(found->hp == expected.value("hp", -1));
        CHECK(found->contact_damage ==
              expected.value("contact_damage", -1));
        CHECK(found->projectile_damage ==
              expected.value("projectile_damage", -1));
        CHECK(found->death_explosion_damage ==
              expected.value("death_explosion_damage", -1));
        if (expected.contains("contact_cooldown")) {
            CHECK(found->contact_cooldown ==
                  doctest::Approx(expected["contact_cooldown"].get<float>()));
        }
        ++compared;
    }
    CHECK(compared == reference.value("enemy_count", 0));
    CHECK(compared == 101);
}

TEST_CASE("Known offensive enemy values stay exact" *
          doctest::test_suite("unit")) {
    // These are the concrete recovered values that a generic-default refactor
    // would most likely overwrite. Every entry comes from the APK prefab
    // weapon_atk/controller fields, not from a balancing table.
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    SceneAsset scene;
    std::string error;
    CHECK(SceneLoader::Load(filesystem, "scenes/r1_1_visual/scene.json",
                              scene, &error));
    if (scene.enemy_definitions.empty()) return;
    const auto find = [&](const std::string& id) {
        return std::find_if(
            scene.enemy_definitions.begin(), scene.enemy_definitions.end(),
            [&](const auto& definition) { return definition.id == id; });
    };
    const auto alien01 = find("e_alien01");
    CHECK(alien01 != scene.enemy_definitions.end());
    if (alien01 == scene.enemy_definitions.end()) return;
    CHECK(alien01->contact_damage == 4);
    CHECK(alien01->projectile_damage == 0);
    const auto alien02 = find("e_alien02");
    CHECK(alien02 != scene.enemy_definitions.end());
    CHECK(alien02->contact_damage == 4);
    CHECK(alien02->projectile_damage == 4);
    const auto alien03 = find("e_alien03");
    CHECK(alien03 != scene.enemy_definitions.end());
    CHECK(alien03->contact_damage == 5);
    CHECK(alien03->projectile_damage == 5);
    const auto boar = find("e_boar01");
    CHECK(boar != scene.enemy_definitions.end());
    if (boar == scene.enemy_definitions.end()) return;
    CHECK(boar->contact_damage == 2);
}

TEST_CASE("Damage reference is tied to the recovered scene and catalogue" *
          doctest::test_suite("unit")) {
    const Json reference = LoadDamageReference();
    CHECK(reference.value("schema", 0) == 1);
    CHECK(reference.value("source_scene", "") ==
          "analysis/assets/scenes/r1_1_visual.json");
    CHECK(reference.value("source_scene_sha256", "").size() == 64);
    CHECK(reference.value("source_catalogue_sha256", "").size() == 64);
    CHECK(reference.value("classification", "").find("CONFIRMED") !=
          std::string::npos);
}
