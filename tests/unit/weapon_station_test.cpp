// Soul Knight Linux — weapon station blueprint/material crafting
#include <doctest/doctest.h>

#include "game/hub/Workshop.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <string>

using soulknight::game::hub::Workshop;
using soulknight::platform::FileSystem;

TEST_CASE("Weapon station never gives a free weapon" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    Workshop workshop;
    std::string error;
    CHECK(workshop.Load(filesystem, "scenes/items/weapon_materials.json",
                        &error));
    CHECK(workshop.Recipes().size() >= 200);
    const auto* pistol = workshop.Find("weapon_000");
    CHECK(pistol != nullptr);
    if (pistol == nullptr) return;
    CHECK_FALSE(workshop.HasMaterials("weapon_000"));
    CHECK_FALSE(workshop.CanCraft("weapon_000"));
    CHECK_FALSE(workshop.ConsumeForCraft("weapon_000"));
    for (const auto& material : pistol->materials) {
        workshop.AddMaterial(material.id, material.count);
    }
    CHECK(workshop.HasMaterials("weapon_000"));
    CHECK(workshop.CanCraft("weapon_000"));
    CHECK(workshop.ConsumeForCraft("weapon_000"));
    CHECK_FALSE(workshop.HasMaterials("weapon_000"));
    CHECK_FALSE(workshop.CanCraft("weapon_000"));
}

TEST_CASE("Blueprint-locked weapon cannot be crafted until collected" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    Workshop workshop;
    std::string error;
    CHECK(workshop.Load(filesystem, "scenes/items/weapon_materials.json",
                        &error));
    const auto* locked = workshop.Find("weapon_184");
    CHECK(locked != nullptr);
    if (locked == nullptr) return;
    CHECK(workshop.RequiresBlueprint("weapon_184"));
    CHECK_FALSE(workshop.HasBlueprint("weapon_184"));
    for (const auto& material : locked->materials) {
        workshop.AddMaterial(material.id, material.count);
    }
    CHECK(workshop.HasMaterials("weapon_184"));
    CHECK_FALSE(workshop.CanCraft("weapon_184"));
    CHECK_FALSE(workshop.ConsumeForCraft("weapon_184"));
    workshop.SetUnlockedBlueprints({"weapon_184"});
    CHECK(workshop.HasBlueprint("weapon_184"));
    CHECK(workshop.CanCraft("weapon_184"));
    CHECK(workshop.ConsumeForCraft("weapon_184"));
    CHECK_FALSE(workshop.HasMaterials("weapon_184"));
    CHECK(workshop.UnlockedBlueprints() ==
          std::vector<std::string>{"weapon_184"});
}

TEST_CASE("Blueprint state persists with material stock" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    Workshop workshop;
    std::string error;
    CHECK(workshop.Load(filesystem, "scenes/items/weapon_materials.json",
                        &error));
    workshop.AddMaterial("material_iron", 9);
    workshop.SetUnlockedBlueprints({"weapon_188", "weapon_294"});
    const auto materials = workshop.SnapshotMaterials();
    const auto blueprints = workshop.UnlockedBlueprints();

    Workshop restored;
    CHECK(restored.Load(filesystem, "scenes/items/weapon_materials.json",
                        &error));
    restored.RestoreMaterials(materials);
    restored.SetUnlockedBlueprints(blueprints);
    CHECK(restored.MaterialCount("material_iron") == 9);
    CHECK(restored.HasBlueprint("weapon_188"));
    CHECK(restored.HasBlueprint("weapon_294"));
    CHECK_FALSE(restored.HasBlueprint("weapon_184"));
    CHECK(restored.CanCraft("weapon_188"));
    CHECK_FALSE(restored.CanCraft("weapon_184"));
}
