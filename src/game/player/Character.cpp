// Soul Knight Linux — Character
// UNIMPLEMENTED PLACEHOLDER: logs only. No reference character behavior is modeled.
#include "Character.hpp"
#include "engine/core/Log.hpp"

namespace soulknight::game {

void Character::Initialize(const CharacterData& data) {
    (void)data; // UNIMPLEMENTED: no reference-derived character data is applied yet.
    LOG_DEBUG_GAME("Character initialized: {}", data.id);
}

} // namespace soulknight::game