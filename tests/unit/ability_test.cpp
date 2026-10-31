// Soul Knight Linux — ability/energy architecture tests
#include <doctest/doctest.h>

#include "game/player/Ability.hpp"

using soulknight::game::player::AbilityController;
using soulknight::game::player::AbilityDefinition;
using soulknight::game::player::AbilityState;
using soulknight::game::player::EnergyState;

TEST_CASE("Ability consumes energy and enters active/cooldown states" *
          doctest::test_suite("unit")) {
    AbilityController abilities;
    AbilityDefinition def;
    def.id = "dev_power";
    def.energy_cost = 10;
    def.active_time = 0.5f;
    def.cooldown = 2.0f;
    def.development_placeholder = true;
    abilities.Add(def);
    EnergyState energy;
    energy.Reset(180, 15);
    CHECK(abilities.Activate(0, energy));
    CHECK(energy.energy == 5);
    CHECK(abilities.Abilities()[0].state == AbilityState::Active);
    abilities.Update(0.5f);
    CHECK(abilities.Abilities()[0].state == AbilityState::Cooldown);
    abilities.Update(2.0f);
    CHECK(abilities.Abilities()[0].state == AbilityState::Ready);
}

TEST_CASE("Ability rejects insufficient energy and invalid indices" *
          doctest::test_suite("unit")) {
    AbilityController abilities;
    AbilityDefinition def;
    def.id = "costly";
    def.energy_cost = 20;
    abilities.Add(def);
    EnergyState energy;
    energy.Reset(180, 5);
    CHECK_FALSE(abilities.Activate(0, energy));
    CHECK(energy.energy == 5);
    CHECK_FALSE(abilities.Activate(5, energy));
    CHECK(abilities.Count() == 1);
    CHECK(abilities.ReadyCount() == 1);
}
