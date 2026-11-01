#include "Workshop.hpp"

#include <algorithm>

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

namespace soulknight::game::hub {
namespace {
using Json = nlohmann::json;

void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) *error = message;
}

// Recovered BluePrint weapon prefabs. These 17 weapons are gated by a
// collected blueprint (`blueprint_weapon_<id>`) in the reference data.
const std::unordered_set<std::string> kBlueprintWeapons = {
    "weapon_184", "weapon_188", "weapon_189", "weapon_195",
    "weapon_211", "weapon_212", "weapon_213", "weapon_217",
    "weapon_223", "weapon_224", "weapon_237", "weapon_238",
    "weapon_240", "weapon_242", "weapon_245", "weapon_252",
    "weapon_294"};
} // namespace

bool Workshop::Load(const platform::FileSystem& filesystem,
                    const std::filesystem::path& path, std::string* error) {
    loaded_ = false;
    recipes_.clear();
    if (error != nullptr) error->clear();
    const auto text = filesystem.ReadTextFile(path);
    if (!text) {
        SetError(error, "workshop recipe table not found: " + path.string());
        return false;
    }
    const Json document = Json::parse(*text, nullptr, false);
    if (document.is_discarded() || !document.is_object() ||
        !document.contains("weapons") || !document["weapons"].is_array()) {
        SetError(error, "workshop recipe table is invalid");
        return false;
    }
    for (const Json& item : document["weapons"]) {
        if (!item.is_object() || !item.contains("weapon_id") ||
            !item["weapon_id"].is_string()) {
            continue;
        }
        WeaponRecipe recipe;
        recipe.weapon_id = item["weapon_id"].get<std::string>();
        recipe.name = item.contains("name") && item["name"].is_string()
            ? item["name"].get<std::string>() : recipe.weapon_id;
        recipe.source = item.contains("source") && item["source"].is_string()
            ? item["source"].get<std::string>() : std::string{};
        recipe.item_level = item.value("item_level", 0);
        recipe.weapon_type = item.value("weapon_type", 0);
        recipe.item_value = item.value("item_value", 0);
        if (item.contains("materials") && item["materials"].is_array()) {
            for (const Json& material_json : item["materials"]) {
                if (!material_json.is_object() ||
                    !material_json.contains("id") ||
                    !material_json["id"].is_string()) {
                    continue;
                }
                CraftMaterial material;
                material.id = material_json["id"].get<std::string>();
                material.count = material_json.value("count", 0);
                if (material.count > 0) {
                    recipe.materials.push_back(std::move(material));
                }
            }
        }
        if (!recipe.materials.empty()) {
            recipes_.push_back(std::move(recipe));
        }
    }
    material_names_.clear();
    const std::filesystem::path materials_path =
        path.parent_path() / "workshop_materials.json";
    if (const auto material_text = filesystem.ReadTextFile(materials_path)) {
        const Json material_document =
            Json::parse(*material_text, nullptr, false);
        if (!material_document.is_discarded() &&
            material_document.contains("materials") &&
            material_document["materials"].is_array()) {
            for (const Json& material : material_document["materials"]) {
                if (!material.is_object() || !material.contains("id") ||
                    !material["id"].is_string()) {
                    continue;
                }
                const std::string id = material["id"].get<std::string>();
                const std::string english =
                    material.contains("display_name_en") &&
                            material["display_name_en"].is_string()
                        ? material["display_name_en"].get<std::string>()
                        : std::string{};
                const std::string chinese =
                    material.contains("display_name_zh") &&
                            material["display_name_zh"].is_string()
                        ? material["display_name_zh"].get<std::string>()
                        : std::string{};
                material_names_[id] = !english.empty() ? english
                    : (!chinese.empty() ? chinese : id);
            }
        }
    }
    loaded_ = !recipes_.empty();
    if (!loaded_) {
        SetError(error, "workshop recipe table has no usable recipes");
        return false;
    }
    return true;
}

const WeaponRecipe* Workshop::Find(const std::string& weapon_id) const {
    for (const WeaponRecipe& recipe : recipes_) {
        if (recipe.weapon_id == weapon_id) return &recipe;
    }
    return nullptr;
}

int Workshop::MaterialCount(const std::string& material_id) const {
    const auto it = materials_.find(material_id);
    return it == materials_.end() ? 0 : it->second;
}

std::string Workshop::MaterialName(const std::string& material_id) const {
    const auto it = material_names_.find(material_id);
    return it == material_names_.end() ? material_id : it->second;
}

bool Workshop::HasMaterials(const std::string& weapon_id) const {
    const WeaponRecipe* recipe = Find(weapon_id);
    if (recipe == nullptr || recipe->materials.empty()) return false;
    for (const CraftMaterial& material : recipe->materials) {
        if (MaterialCount(material.id) < material.count) return false;
    }
    return true;
}

bool Workshop::RequiresBlueprint(const std::string& weapon_id) const {
    return kBlueprintWeapons.contains(weapon_id);
}

bool Workshop::HasBlueprint(const std::string& weapon_id) const {
    return unlocked_blueprints_.contains(weapon_id);
}

void Workshop::SetUnlockedBlueprints(
        const std::vector<std::string>& blueprints) {
    unlocked_blueprints_.clear();
    for (const std::string& id : blueprints) {
        if (!id.empty()) unlocked_blueprints_.insert(id);
    }
}

std::vector<std::string> Workshop::UnlockedBlueprints() const {
    std::vector<std::string> result(unlocked_blueprints_.begin(),
                                    unlocked_blueprints_.end());
    std::sort(result.begin(), result.end());
    return result;
}

bool Workshop::CanCraft(const std::string& weapon_id) const {
    if (RequiresBlueprint(weapon_id) && !HasBlueprint(weapon_id)) {
        return false;
    }
    return HasMaterials(weapon_id);
}

bool Workshop::ConsumeForCraft(const std::string& weapon_id) {
    const WeaponRecipe* recipe = Find(weapon_id);
    if (recipe == nullptr || !CanCraft(weapon_id)) return false;
    for (const CraftMaterial& material : recipe->materials) {
        materials_[material.id] -= material.count;
    }
    return true;
}

const WeaponRecipe* Workshop::AddMaterialsForDismantle(
        const std::string& weapon_id) {
    const WeaponRecipe* recipe = Find(weapon_id);
    if (recipe == nullptr) return nullptr;
    for (const CraftMaterial& material : recipe->materials) {
        materials_[material.id] += material.count;
    }
    return recipe;
}

void Workshop::AddMaterial(const std::string& material_id, int count) {
    if (material_id.empty() || count == 0) return;
    materials_[material_id] = std::max(0, MaterialCount(material_id) + count);
}

void Workshop::Reset() {
    materials_.clear();
}

std::vector<CraftMaterial> Workshop::SnapshotMaterials() const {
    std::vector<CraftMaterial> result;
    result.reserve(materials_.size());
    for (const auto& [id, count] : materials_) {
        if (count > 0) result.push_back({id, count});
    }
    std::sort(result.begin(), result.end(),
              [](const CraftMaterial& a, const CraftMaterial& b) {
                  return a.id < b.id;
              });
    return result;
}

void Workshop::RestoreMaterials(const std::vector<CraftMaterial>& materials) {
    materials_.clear();
    for (const CraftMaterial& material : materials) {
        if (material.id.empty() || material.count <= 0) continue;
        materials_[material.id] = material.count;
    }
}

} // namespace soulknight::game::hub
