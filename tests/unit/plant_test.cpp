// Soul Knight Linux — Living Room plant foundation tests
#include <doctest/doctest.h>

#include "game/hub/PlantSystem.hpp"

using soulknight::engine::Vec2;
using soulknight::game::hub::PlantGrowthState;
using soulknight::game::hub::PlantInteraction;
using soulknight::game::hub::PlantSystem;

TEST_CASE("Plant plot grows deterministically and can be harvested" *
          doctest::test_suite("unit")) {
    PlantSystem plants;
    plants.AddPlot("plot_a", {0.0f, 0.0f}, 2.0f);
    plants.AddPlot("plot_b", {3.0f, 0.0f}, 2.0f);
    plants.AddSeed("carrot", 2);
    CHECK(plants.NearestPlot({0.1f, 0.0f}, 1.0f) == 0);
    CHECK(plants.NearestPlot({10.0f, 0.0f}, 1.0f) == -1);
    CHECK(plants.Interact(0, "carrot") == PlantInteraction::Planted);
    CHECK(plants.SeedCount("carrot") == 1);
    CHECK(plants.Plots()[0].state == PlantGrowthState::Growing);
    plants.Update(1.0f);
    CHECK(plants.Plots()[0].state == PlantGrowthState::Growing);
    CHECK(plants.Interact(0, "carrot") == PlantInteraction::Watered);
    CHECK(plants.Interact(0, "carrot") == PlantInteraction::NotReady);
    plants.Update(1.0f);
    CHECK(plants.Plots()[0].state == PlantGrowthState::Ready);
    CHECK(plants.Interact(0, "") == PlantInteraction::Harvested);
    CHECK(plants.Plots()[0].state == PlantGrowthState::Empty);
    CHECK(plants.HarvestedCount() == 1);
    CHECK(plants.ActiveCount() == 0);
}

TEST_CASE("Plant growth ignores invalid deltas and reset clears state" *
          doctest::test_suite("unit")) {
    PlantSystem plants;
    plants.AddPlot("plot", {0.0f, 0.0f}, 1.0f);
    plants.AddSeed("seed", 1);
    CHECK(plants.Interact(0, "seed") == PlantInteraction::Planted);
    plants.Update(-1.0f);
    CHECK(plants.Plots()[0].elapsed == doctest::Approx(0.0f));
    plants.Update(2.0f);
    CHECK(plants.Plots()[0].state == PlantGrowthState::Ready);
    plants.Reset();
    CHECK(plants.Plots()[0].state == PlantGrowthState::Empty);
    CHECK(plants.ActiveCount() == 0);
    CHECK(plants.HarvestedCount() == 0);
}

TEST_CASE("Invalid plant interactions are rejected" *
          doctest::test_suite("unit")) {
    PlantSystem plants;
    plants.AddPlot("plot", {});
    CHECK(plants.Interact(-1, "seed") == PlantInteraction::None);
    CHECK(plants.Interact(5, "seed") == PlantInteraction::None);
    CHECK(plants.NearestPlot({}, 0.0f) == -1);
    CHECK(plants.Interact(0, "") == PlantInteraction::NoSeed);
    plants.AddSeed("seed", 1);
    CHECK(plants.Interact(0, "seed") == PlantInteraction::Planted);
    CHECK(plants.Interact(0, "seed") == PlantInteraction::Watered);
    CHECK(plants.Interact(0, "seed") == PlantInteraction::NotReady);
}

TEST_CASE("Locked Living Room planter rejects planting" *
          doctest::test_suite("unit")) {
    PlantSystem plants;
    plants.AddPlot("locked", {0.0f, 0.0f}, 1.0f, true, 4);
    plants.AddSeed("plant_carrot", 1);
    CHECK(plants.Interact(0, "plant_carrot") == PlantInteraction::Locked);
    CHECK(plants.Plots()[0].state == PlantGrowthState::Empty);
    CHECK(plants.SeedCount("plant_carrot") == 1);
}

TEST_CASE("Planting water and harvested state survive save/load" *
          doctest::test_suite("unit")) {
    PlantSystem plants;
    plants.AddPlot("plot_a", {1.0f, 2.0f}, 4.0f, false, 4);
    plants.AddPlot("plot_b", {4.0f, 2.0f}, 4.0f, true, 4);
    plants.AddSeed("plant_carrot", 3);
    CHECK(plants.Interact(0, "plant_carrot", 4.0f, 4,
                          "weapon_177", 1) == PlantInteraction::Planted);
    CHECK(plants.Interact(0, "plant_carrot") == PlantInteraction::Watered);
    plants.Update(1.0f);

    PlantSystem restored;
    restored.AddPlot("plot_a", {1.0f, 2.0f}, 4.0f, false, 4);
    restored.AddPlot("plot_b", {4.0f, 2.0f}, 4.0f, true, 4);
    CHECK(restored.RestorePlots(plants.SerializePlots()));
    CHECK(restored.RestoreSeeds(plants.SerializeSeeds()));
    CHECK(restored.Plots().size() == 2);
    if (restored.Plots().size() < 2) return;
    CHECK(restored.Plots()[0].state == PlantGrowthState::Growing);
    CHECK(restored.Plots()[0].species == "plant_carrot");
    CHECK(restored.Plots()[0].watered);
    CHECK(restored.Plots()[0].harvest_item == "weapon_177");
    CHECK(restored.SeedCount("plant_carrot") == 2);
    CHECK(restored.Plots()[1].locked);

    restored.Update(3.0f);
    CHECK(restored.Plots()[0].state == PlantGrowthState::Ready);
    CHECK(restored.Interact(0, "") == PlantInteraction::Harvested);
    CHECK(restored.HarvestedCount() == 1);
}
