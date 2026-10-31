// Soul Knight Linux — Character
#pragma once

#include <string>
#include <vector>
#include <engine/core/Types.hpp>

namespace soulknight::game {

// UNIMPLEMENTED SCAFFOLDING: these defaults are intentionally zero. No
// reference character stats have been reconstructed, so populating plausible
// values here would be fabricated behavior. Do not use this data in gameplay.
struct CharacterData {
    std::string id;
    std::string name;
    int base_health = 0;
    int base_energy = 0;
    float move_speed = 0.0f;
    std::string starting_weapon;
    std::string active_skill;
    std::vector<std::string> passive_skills;
    std::string sprite_path;
};

class Character {
public:
    void Initialize(const CharacterData& data);

private:
    CharacterData data_;
};

} // namespace soulknight::game