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
    CHECK(plants.NearestPlot({0.1f, 0.0f}, 1.0f) == 0);
    CHECK(plants.NearestPlot({10.0f, 0.0f}, 1.0f) == -1);
    CHECK(plants.Interact(0, "carrot") == PlantInteraction::Planted);
    CHECK(plants.Plots()[0].state == PlantGrowthState::Growing);
    plants.Update(1.0f);
    CHECK(plants.Plots()[0].state == PlantGrowthState::Growing);
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
}
