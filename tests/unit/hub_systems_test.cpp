// Soul Knight Linux — recovered pets, stake, boxes and workshop tests
#include <doctest/doctest.h>

#include "game/enemy/EnemySystem.hpp"
#include "game/hub/TrainingStake.hpp"
#include "game/hub/Workshop.hpp"
#include "game/room/BiomeSystem.hpp"
#include "game/items/BoxSystem.hpp"
#include "game/pet/PetSystem.hpp"
#include "game/room/ObstacleField.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <cmath>

using soulknight::engine::Vec2;
using soulknight::engine::assets::EnemyDefinition;
using soulknight::engine::assets::ObstacleMarker;
using soulknight::engine::assets::ObstacleVariant;
using soulknight::engine::assets::PetDefinition;
using soulknight::engine::assets::PetFrame;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::assets::TrainingStake;
using soulknight::game::enemy::EnemySystem;
using soulknight::game::hub::Workshop;
using soulknight::game::room::BiomeSystem;
using soulknight::game::items::BoxSystem;
using soulknight::game::pet::PetSystem;
using soulknight::game::room::ObstacleField;
using soulknight::platform::FileSystem;

namespace {

SceneAsset MakePetScene() {
    SceneAsset scene;
    PetDefinition pet;
    pet.id = "pet";
    pet.display_name = "Cat";
    pet.max_hp = 10;
    pet.damage = 3;
    pet.speed = 8.0f;
    pet.atk_cd = 2.0f;
    pet.min_follow_distance = 2.0f;
    pet.max_follow_distance = 20.0f;
    pet.scout_distance = 8.0f;
    pet.atk_range = 2.0f;
    pet.body_sprite_index = 0;
    pet.idle_frames.push_back(PetFrame{0.0f, 0});
    scene.pet_definitions.push_back(pet);

    EnemyDefinition enemy;
    enemy.id = "e_test";
    enemy.hp = 12;
    enemy.camp = 0;
    enemy.speed = 0;
    enemy.contact_damage = 2;
    enemy.contact_cooldown = 1.5f;
    enemy.collider.min = {-0.3f, 0.0f};
    enemy.collider.max = {0.3f, 1.0f};
    scene.enemy_definitions.push_back(enemy);
    return scene;
}

} // namespace

TEST_CASE("Pet follows the master, teleports at max distance and attacks" *
          doctest::test_suite("unit")) {
    const SceneAsset scene = MakePetScene();
    PetSystem pet;
    std::string error;
    CHECK(pet.Configure(scene, &error));
    CHECK(pet.Count() == 1);
    CHECK(pet.SelectedId() == "pet");

    EnemySystem enemies;
    CHECK(enemies.Configure(scene, &error));
    CHECK(enemies.SpawnEnemy(0, {3.0f, 0.0f}));
    CHECK(pet.Update(0.5f, {0.0f, 0.0f}, false, enemies, nullptr) ==
          enemies.Enemies().size());
    CHECK(enemies.Enemies()[0].health.hp == 12);
    // Within attack range after moving toward the target.
    for (int i = 0; i < 30; ++i) {
        (void)pet.Update(0.05f, {0.0f, 0.0f}, false, enemies, nullptr);
    }
    CHECK(enemies.Enemies()[0].health.hp < 12);
    CHECK(pet.CurrentSprite() >= 0);

    // Beyond max_follow_distance the recovered controller teleports when no
    // target is being scouted.
    enemies.ClearEnemies();
    (void)pet.Update(0.05f, {100.0f, 0.0f}, false, enemies, nullptr);
    CHECK(std::fabs(pet.Position().x - 100.0f) < 2.5f);
}

TEST_CASE("Training stake resets HP on every hit and uses the recovered clip" *
          doctest::test_suite("unit")) {
    SceneAsset scene;
    scene.has_training_stake = true;
    scene.training_stake.id = "stake";
    scene.training_stake.hp = 40;
    scene.training_stake.position = {0.0f, 0.0f};
    scene.training_stake.body_sprite_index = 4;
    scene.training_stake.frames.push_back(PetFrame{0.0f, 5});
    scene.training_stake.frames.push_back(PetFrame{0.2f, 6});
    soulknight::game::hub::TrainingStake stake;
    CHECK(stake.Configure(scene));
    CHECK(stake.Hit(17, 5.0f));
    CHECK(stake.FacingLeft());
    CHECK(stake.CurrentSprite() == 5);
    stake.Update(0.1f);
    CHECK(stake.CurrentSprite() == 5);
    stake.Update(0.2f);
    CHECK(stake.CurrentSprite() == 6);
    CHECK(stake.Hit(999, -5.0f));
    CHECK_FALSE(stake.FacingLeft());
    CHECK(stake.HitTimer() > 0.0f);
}

TEST_CASE("Breakable RGBox markers drop RGEnergy bubbles" *
          doctest::test_suite("unit")) {
    SceneAsset scene;
    ObstacleVariant variant;
    variant.name = "box04";
    variant.has_collider = true;
    variant.collider = {{-0.4f, 0.0f}, {0.4f, 1.0f}};
    variant.visuals.push_back({0, {0.0f, 0.0f}, 0.0f, {1.0f, 1.0f},
                               {255, 255, 255, 255}, 4, 0, false, false});
    variant.breakable = true;
    variant.hp = 4;
    scene.obstacle_variants.push_back(variant);
    ObstacleMarker marker;
    marker.position = {2.0f, 2.0f};
    marker.variant = 0;
    scene.obstacle_markers.push_back(marker);

    ObstacleField obstacles;
    std::string error;
    CHECK(obstacles.Configure(scene, &error));
    BoxSystem boxes;
    boxes.Configure(scene, obstacles);
    CHECK(boxes.Boxes().size() == 1);
    CHECK(boxes.Boxes()[0].hp == 4);
    CHECK(boxes.ApplyMelee({0.0f, 2.0f}, {1.0f, 0.0f}, 2.5f, 120.0f, 4) == 1);
    CHECK_FALSE(boxes.Boxes()[0].alive);
    CHECK(boxes.Bubbles().size() == 1);
    int absorbed = 0;
    for (int i = 0; i < 200; ++i) {
        absorbed += boxes.Update(0.02f, {2.0f, 2.7f});
    }
    CHECK(absorbed == 8);
}

TEST_CASE("Workshop consumes and restores the recovered material recipe" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    const std::string recipes = R"({
      "weapons": [
        {"weapon_id":"weapon_000","name":"Bad Pistol","source":"WeaponMaterial",
         "material_count":2,
         "materials":[{"id":"material_iron","count":1},
                      {"id":"material_wood","count":2}]}
      ]
    })";
    CHECK(filesystem.WriteFile("test/workshop_test.json", recipes));
    Workshop workshop;
    std::string error;
    CHECK(workshop.Load(filesystem, "test/workshop_test.json", &error));
    CHECK(workshop.Recipes().size() == 1);
    CHECK_FALSE(workshop.CanCraft("weapon_000"));
    workshop.AddMaterial("material_iron", 1);
    workshop.AddMaterial("material_wood", 2);
    CHECK(workshop.CanCraft("weapon_000"));
    CHECK(workshop.ConsumeForCraft("weapon_000"));
    CHECK(workshop.MaterialCount("material_iron") == 0);
    const auto* recipe = workshop.AddMaterialsForDismantle("weapon_000");
    CHECK(recipe != nullptr);
    CHECK(workshop.MaterialCount("material_wood") == 2);
    const auto snapshot = workshop.SnapshotMaterials();
    workshop.Reset();
    workshop.RestoreMaterials(snapshot);
    CHECK(workshop.MaterialCount("material_wood") == 2);
    CHECK(filesystem.DeleteFile("test/workshop_test.json"));
}

TEST_CASE("Biome catalogue preserves level order and weighted enemy pools" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    const std::string catalogue = R"({
      "levels": [
        {"level":"2-1","map_long":3,"chest_level":2,
         "enemy_infos":[{"enemy":"e_a","weight":1},{"enemy":"e_b","weight":3}]},
        {"level":"1-1","map_long":3,"chest_level":1,
         "enemy_infos":[{"enemy":"e_slime","weight":2}]}
      ]
    })";
    CHECK(filesystem.WriteFile("test/biomes_test.json", catalogue));
    BiomeSystem biomes;
    std::string error;
    CHECK(biomes.Load(filesystem, "test/biomes_test.json", &error));
    CHECK(biomes.Count() == 2);
    CHECK(biomes.Current() != nullptr);
    CHECK(biomes.Current()->id == "1-1");
    CHECK(biomes.Advance());
    CHECK(biomes.Current()->id == "2-1");
    CHECK(biomes.Current()->chest_level == 2);
    CHECK(biomes.PickEnemy(0) == "e_a");
    CHECK(biomes.PickEnemy(1) == "e_b");
    CHECK(biomes.PickEnemy(3) == "e_b");
    CHECK(filesystem.DeleteFile("test/biomes_test.json"));
}
