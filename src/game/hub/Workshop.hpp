// Soul Knight Linux — recovered forge recipes and material inventory
#pragma once

#include "platform/filesystem/FileSystem.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace soulknight::game::hub {

struct CraftMaterial {
    std::string id;
    int count = 0;
};

struct WeaponRecipe {
    std::string weapon_id;
    std::string name;
    std::string source;
    int item_level = 0;
    int weapon_type = 0;
    int item_value = 0;
    std::vector<CraftMaterial> materials;
};

// ItemData.materials / forgeWeapons equivalent. Recipe costs come from the
// recovered WeaponMaterial / CustomWeaponMaterial / DefaultWeaponMaterial
// components; no cost is generated or guessed here.
class Workshop {
public:
    bool Load(const platform::FileSystem& filesystem,
              const std::filesystem::path& path, std::string* error);
    bool IsLoaded() const { return loaded_; }

    const std::vector<WeaponRecipe>& Recipes() const { return recipes_; }
    const WeaponRecipe* Find(const std::string& weapon_id) const;
    int MaterialCount(const std::string& material_id) const;
    std::string MaterialName(const std::string& material_id) const;
    bool HasMaterials(const std::string& weapon_id) const;
    bool RequiresBlueprint(const std::string& weapon_id) const;
    bool HasBlueprint(const std::string& weapon_id) const;
    void SetUnlockedBlueprints(const std::vector<std::string>& blueprints);
    std::vector<std::string> UnlockedBlueprints() const;
    bool CanCraft(const std::string& weapon_id) const;
    bool ConsumeForCraft(const std::string& weapon_id);
    // Reforging/resetting furnace: a forged weapon turns back into its exact
    // recovered material recipe. Returns the recipe or null.
    const WeaponRecipe* AddMaterialsForDismantle(const std::string& weapon_id);
    void AddMaterial(const std::string& material_id, int count);
    void Reset();

    std::vector<CraftMaterial> SnapshotMaterials() const;
    void RestoreMaterials(const std::vector<CraftMaterial>& materials);

private:
    std::vector<WeaponRecipe> recipes_;
    std::unordered_map<std::string, int> materials_;
    std::unordered_map<std::string, std::string> material_names_;
    std::unordered_set<std::string> unlocked_blueprints_;
    bool loaded_ = false;
};

} // namespace soulknight::game::hub
