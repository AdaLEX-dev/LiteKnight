// Soul Knight Linux — full recovered weapon catalogue index
#pragma once

#include "platform/filesystem/FileSystem.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace soulknight::game::weapons {

struct WeaponCatalogueEntry {
    std::string id;
    std::string display_name;
    std::string type_name;
    int damage = 0;
    int energy_cost = 0;
    int item_level = 0;
    bool has_serialized_stats = false;
    std::string tier;
};

// Loads the committed 302-entry reference catalogue. The runtime WeaponRegistry
// remains the authority for Tier A weapons; this index is the lookup/name layer
// for the complete recovered data set and the explicit Tier B/C classification.
class WeaponCatalogue {
public:
    bool Load(const platform::FileSystem& filesystem,
              const std::filesystem::path& path, std::string* error);
    bool IsLoaded() const { return loaded_; }
    std::size_t Count() const { return entries_.size(); }
    const WeaponCatalogueEntry* Find(const std::string& id) const;
    void MarkRuntimeIntegrated(const std::string& id);
    const std::vector<WeaponCatalogueEntry>& Entries() const { return entries_; }

private:
    bool loaded_ = false;
    std::vector<WeaponCatalogueEntry> entries_;
    std::unordered_map<std::string, std::size_t> index_;
};

} // namespace soulknight::game::weapons
