// Soul Knight Linux — Character (Placeholder)
#include "Character.hpp"
#include "engine/core/Log.hpp"

namespace soulknight::game {

void Character::Initialize(const CharacterData& data) {
    LOG_DEBUG_GAME("Character initialized: {}", data.id);
}

} // namespace soulknight::game