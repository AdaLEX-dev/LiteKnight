// Soul Knight Linux — versioned JSON save serializer/migrations
#pragma once

#include "SaveData.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

#include <string>

namespace soulknight::game::save {

using Json = nlohmann::json;

// Native schema only. Unknown versions are rejected rather than guessed.
bool MigrateToCurrent(Json& document, std::string* error);

Json Serialize(const SaveData& data);
bool Deserialize(const Json& document, SaveData* data, std::string* error);

} // namespace soulknight::game::save
