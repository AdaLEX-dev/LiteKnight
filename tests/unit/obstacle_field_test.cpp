// Soul Knight Linux — recovered obstacle variant tests
#include <doctest/doctest.h>

#include "engine/assets/SceneAsset.hpp"
#include "engine/collision/AabbCollision.hpp"
#include "game/room/ObstacleField.hpp"
#include "platform/filesystem/FileSystem.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

#include <string>

using soulknight::engine::assets::ObstacleMarker;
using soulknight::engine::assets::ObstacleVariant;
using soulknight::engine::assets::ObstacleVisual;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::collision::AabbCollisionWorld;
using soulknight::game::room::ObstacleField;
using soulknight::platform::FileSystem;
using Json = nlohmann::json;

namespace {

SceneAsset LoadObstacleScene() {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    const auto text = filesystem.ReadTextFile("test/scenes/r1_1_visual.json");
    CHECK(text.has_value());
    const Json document = Json::parse(text.value_or("{}"), nullptr, false);
    CHECK_FALSE(document.is_discarded());

    SceneAsset scene;
    const Json& obstacles = document["obstacles"];
    for (const Json& item : obstacles["variants"]) {
        ObstacleVariant variant;
        variant.index = item.value("index", 0);
        variant.name = item.value("name", std::string{});
        variant.path_id = item.value("path_id", std::string{});
        const Json& collider = item["collider"];
        if (!collider.is_null()) {
            const float width = collider["size"].at(0).get<float>();
            const float height = collider["size"].at(1).get<float>();
            const float offset_x = collider["offset"].at(0).get<float>();
            const float offset_y = collider["offset"].at(1).get<float>();
            variant.has_collider = true;
            variant.collider_trigger = collider.value("trigger", false);
            variant.collider.min = {
                offset_x - width * 0.5f, offset_y - height * 0.5f
            };
            variant.collider.max = {
                offset_x + width * 0.5f, offset_y + height * 0.5f
            };
        }
        for (std::size_t i = 0; i < item["visuals"].size(); ++i) {
            ObstacleVisual visual;
            visual.sprite_index = static_cast<int32_t>(i);
            variant.visuals.push_back(visual);
        }
        scene.obstacle_variants.push_back(variant);
    }
    for (const Json& item : obstacles["markers"]) {
        ObstacleMarker marker;
        marker.position = {item["position"].at(0).get<float>(),
                           item["position"].at(1).get<float>()};
        marker.variant = item.value("obj_index", 0);
        marker.name = item.value("name", std::string{});
        marker.path = item.value("path", std::string{});
        scene.obstacle_markers.push_back(marker);
    }
    return scene;
}

} // namespace

TEST_CASE("Every recovered obstacle variant keeps render and collision cells synchronized" *
          doctest::test_suite("unit")) {
    const SceneAsset scene = LoadObstacleScene();
    ObstacleField field;
    std::string error;
    CHECK(field.Configure(scene, &error));
    CHECK(error.empty());
    CHECK(field.Markers().size() == 49);
    CHECK(field.Variants().size() == 8);
    CHECK(field.ActiveVariant().name == "wall603");
    CHECK(field.RenderInstanceCount() == 147);
    CHECK(field.ColliderCellCount() == 49);

    for (std::size_t index = 0; index < field.Variants().size(); ++index) {
        CAPTURE(field.Variants()[index].name);
        CHECK(field.Select(index));
        const ObstacleVariant& variant = field.ActiveVariant();
        CHECK(field.RenderInstanceCount() ==
              field.Markers().size() * variant.visuals.size());

        AabbCollisionWorld world;
        field.AddColliders(world);
        const std::size_t expected_colliders = field.Markers().size();
        CHECK(variant.has_collider);
        CHECK_FALSE(variant.collider_trigger);
        CHECK(world.Solids().size() == expected_colliders);
        CHECK(field.ColliderCellCount() == expected_colliders);

        for (std::size_t i = 0; i < field.Markers().size(); ++i) {
            const auto& marker = field.Markers()[i];
            const auto& solid = world.Solids()[i];
            CHECK(solid.min.x == doctest::Approx(
                      marker.position.x + variant.collider.min.x));
            CHECK(solid.min.y == doctest::Approx(
                      marker.position.y + variant.collider.min.y));
            CHECK(solid.max.x == doctest::Approx(
                      marker.position.x + variant.collider.max.x));
            CHECK(solid.max.y == doctest::Approx(
                      marker.position.y + variant.collider.max.y));
        }
    }

    CHECK_FALSE(field.Select(field.Variants().size()));
    CHECK(field.ActiveVariant().name == "brazier01");
    field.CycleNext();
    CHECK(field.ActiveVariant().name == "wall603");
}

TEST_CASE("Decorative obstacle markers do not block room loading" *
          doctest::test_suite("unit")) {
    SceneAsset scene = LoadObstacleScene();
    // r1_2/r1_3 serialize skin_trap markers whose prefab has no collider.
    ObstacleMarker decorative;
    decorative.position = {4.0f, 4.0f};
    decorative.variant = 5;
    decorative.name = "skin_trap";
    scene.obstacle_markers.push_back(decorative);
    ObstacleField field;
    std::string error;
    CHECK(field.Configure(scene, &error));
    CHECK(error.empty());
    CHECK(field.Markers().size() == 49);
    CHECK(field.ColliderCellCount() == 49);
}

TEST_CASE("Obstacle variant cycle covers distinct recovered prefab names" *
          doctest::test_suite("unit")) {
    const SceneAsset scene = LoadObstacleScene();
    ObstacleField field;
    std::string error;
    CHECK(field.Configure(scene, &error));

    std::vector<std::string> names;
    for (std::size_t i = 0; i < field.Variants().size(); ++i) {
        CHECK(field.Select(i));
        names.push_back(field.ActiveVariant().name);
    }
    CHECK(names.size() == 8);
    bool has_wall = false;
    bool has_box = false;
    bool has_brazier = false;
    for (const std::string& name : names) {
        if (name == "wall603") has_wall = true;
        if (name == "box04") has_box = true;
        if (name == "brazier01") has_brazier = true;
    }
    CHECK(has_wall);
    CHECK(has_box);
    CHECK(has_brazier);
}
