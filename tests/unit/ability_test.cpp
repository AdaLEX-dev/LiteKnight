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

TEST_CASE("Knight Dual Wield uses recovered 5s active / 10s cooldown" *
          doctest::test_suite("unit")) {
    AbilityController abilities;
    AbilityDefinition dual;
    dual.id = "knight_dual_wield";
    dual.energy_cost = 0;
    dual.active_time = 5.0f;
    dual.cooldown = 10.0f;
    abilities.Add(dual);
    EnergyState energy;
    energy.Reset(180, 0);
    CHECK(abilities.Activate(0, energy));
    CHECK(energy.energy == 0);
    CHECK(abilities.Abilities()[0].state == AbilityState::Active);
    abilities.Update(4.999f);
    CHECK(abilities.Abilities()[0].state == AbilityState::Active);
    abilities.Update(0.002f);
    CHECK(abilities.Abilities()[0].state == AbilityState::Cooldown);
    abilities.Update(9.999f);
    CHECK(abilities.Abilities()[0].state == AbilityState::Cooldown);
    abilities.Update(0.002f);
    CHECK(abilities.Abilities()[0].state == AbilityState::Ready);
}

TEST_CASE("Skill mask charge fills from empty to blue-ready" *
          doctest::test_suite("unit")) {
    CHECK(AbilityChargeFraction(AbilityState::Empty, 0.0f) ==
          doctest::Approx(0.0f));
    CHECK(AbilityChargeFraction(AbilityState::Cooldown, 1.0f) ==
          doctest::Approx(0.0f));
    CHECK(AbilityChargeFraction(AbilityState::Cooldown, 0.5f) ==
          doctest::Approx(0.5f));
    CHECK(AbilityChargeFraction(AbilityState::Cooldown, 0.0f) ==
          doctest::Approx(1.0f));
    CHECK(AbilityChargeFraction(AbilityState::Ready, 0.0f) ==
          doctest::Approx(1.0f));
    CHECK(AbilityChargeFraction(AbilityState::Active, 0.0f) ==
          doctest::Approx(1.0f));
}
