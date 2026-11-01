#include "WeaponCatalogue.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>
#include <cstdio>

namespace soulknight::game::weapons {
namespace {
using Json = nlohmann::json;
void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) *error = message;
}
} // namespace

bool WeaponCatalogue::Load(const platform::FileSystem& filesystem,
                           const std::filesystem::path& path,
                           std::string* error) {
    loaded_ = false;
    entries_.clear();
    index_.clear();
    if (error != nullptr) error->clear();
    const auto text = filesystem.ReadTextFile(path);
    if (!text) {
        SetError(error, "weapon catalogue missing: " + path.string());
        return false;
    }
    const Json document = Json::parse(*text, nullptr, false);
    if (document.is_discarded() || !document.is_object() ||
        !document.contains("weapons") || !document["weapons"].is_array()) {
        SetError(error, "weapon catalogue is malformed");
        return false;
    }
    const Json& weapons_json = document.at("weapons");
    for (const Json& item : weapons_json) {
        if (!item.is_object() || !item.contains("id") ||
            !item["id"].is_string()) {
            continue;
        }
        const auto string_value = [&](const char* key) {
            return item.contains(key) && item[key].is_string()
                ? item[key].get<std::string>() : std::string{};
        };
        const auto int_value = [&](const char* key) {
            return item.contains(key) && item[key].is_number_integer()
                ? item[key].get<int>() : 0;
        };
        WeaponCatalogueEntry entry;
        entry.id = item["id"].get<std::string>();
        entry.display_name = string_value("display_name");
        if (entry.display_name.empty()) entry.display_name = entry.id;
        entry.type_name = string_value("type");
        entry.damage = int_value("damage");
        entry.energy_cost = int_value("energy_cost");
        entry.item_level = int_value("item_level");
        entry.item_value = int_value("item_value");
        entry.has_serialized_stats = item.contains("damage") &&
                                     item["damage"].is_number_integer() &&
                                     item.contains("energy_cost") &&
                                     item["energy_cost"].is_number_integer();
        entry.tier = item.contains("damage") ? "B" : "C";
        index_[entry.id] = entries_.size();
        entries_.push_back(std::move(entry));
    }
    if (entries_.empty()) {
        SetError(error, "weapon catalogue has no entries");
        return false;
    }
    // Tier A entries are identified by the runtime scene loader; this index
    // initially marks stat-bearing entries as B and stat-less entries as C.
    loaded_ = true;
    return true;
}

void WeaponCatalogue::MarkRuntimeIntegrated(const std::string& id) {
    const auto it = index_.find(id);
    if (it != index_.end()) entries_[it->second].tier = "A";
}

const WeaponCatalogueEntry* WeaponCatalogue::Find(const std::string& id) const {
    const auto it = index_.find(id);
    return it == index_.end() ? nullptr : &entries_[it->second];
}

} // namespace soulknight::game::weapons
