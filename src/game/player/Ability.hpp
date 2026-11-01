// Soul Knight Linux — ability/energy resource architecture
#pragma once

#include "Energy.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace soulknight::game::player {

enum class AbilityState {
    Empty,
    Ready,
    Active,
    Cooldown
};

// Ability state machine. c00 installs the recovered Knight Dual Wield values
// (active 5.0 s, serialized cooldown 10.0 s, no energy cost); other characters
// remain unimplemented until their SkillInfo/hand paths are recovered.
struct AbilityDefinition {
    std::string id;
    int energy_cost = 0;
    float active_time = 0.0f;
    float cooldown = 0.0f;
    bool development_placeholder = false;
};

struct AbilityRuntime {
    AbilityDefinition definition;
    AbilityState state = AbilityState::Empty;
    float timer = 0.0f;
};

// UICanvas.UpdateSkillMask fills skill_icon_mask with thisSkillTime/cd and
// enables skill_mask_ready at 1.0. Cooldown fraction is remaining/definition.
inline float AbilityChargeFraction(AbilityState state, float cooldown_fraction) {
    if (state == AbilityState::Ready || state == AbilityState::Active) {
        return 1.0f;
    }
    if (state == AbilityState::Cooldown) {
        return std::clamp(1.0f - cooldown_fraction, 0.0f, 1.0f);
    }
    return 0.0f;
}

class AbilityController {
public:
    void Clear() { abilities_.clear(); }
    void Add(const AbilityDefinition& definition) {
        AbilityRuntime runtime;
        runtime.definition = definition;
        runtime.state = definition.id.empty() ? AbilityState::Empty
                                              : AbilityState::Ready;
        abilities_.push_back(std::move(runtime));
    }

    bool Activate(std::size_t index, EnergyState& energy) {
        if (index >= abilities_.size()) return false;
        AbilityRuntime& ability = abilities_[index];
        if (ability.state != AbilityState::Ready) return false;
        if (!energy.CanConsume(ability.definition.energy_cost)) return false;
        if (ability.definition.energy_cost > 0) {
            (void)energy.Consume(ability.definition.energy_cost);
        }
        ability.state = ability.definition.active_time > 0.0f
            ? AbilityState::Active : AbilityState::Cooldown;
        ability.timer = ability.state == AbilityState::Active
            ? ability.definition.active_time : ability.definition.cooldown;
        return true;
    }

    void Update(float delta_seconds) {
        if (delta_seconds <= 0.0f) return;
        for (AbilityRuntime& ability : abilities_) {
            if (ability.state == AbilityState::Active) {
                ability.timer = std::max(0.0f, ability.timer - delta_seconds);
                if (ability.timer <= 0.0f) {
                    if (ability.definition.cooldown > 0.0f) {
                        ability.state = AbilityState::Cooldown;
                        ability.timer = ability.definition.cooldown;
                    } else {
                        ability.state = AbilityState::Ready;
                    }
                }
            } else if (ability.state == AbilityState::Cooldown) {
                ability.timer = std::max(0.0f, ability.timer - delta_seconds);
                if (ability.timer <= 0.0f) {
                    ability.state = AbilityState::Ready;
                }
            }
        }
    }

    const std::vector<AbilityRuntime>& Abilities() const { return abilities_; }
    std::size_t Count() const { return abilities_.size(); }
    std::size_t ReadyCount() const {
        std::size_t count = 0;
        for (const AbilityRuntime& ability : abilities_) {
            if (ability.state == AbilityState::Ready ||
                ability.state == AbilityState::Active) {
                ++count;
            }
        }
        return count;
    }

private:
    std::vector<AbilityRuntime> abilities_;
};

} // namespace soulknight::game::player
