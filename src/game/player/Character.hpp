// Soul Knight Linux — Character
#pragma once

#include <string>
#include <vector>
#include <engine/core/Types.hpp>

namespace soulknight::game {

struct CharacterData {
    std::string id;
    std::string name;
    int base_health = 100;
    int base_energy = 100;
    float move_speed = 5.0f;
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