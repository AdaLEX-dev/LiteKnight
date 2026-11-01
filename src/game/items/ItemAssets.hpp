// Soul Knight Linux — recovered chest/coin item asset pack
#pragma once

#include "engine/assets/SceneAsset.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace soulknight::game::items {

struct ItemVisual {
    std::string sprite;
    engine::Vec2 offset{0.0f, 0.0f};
    int32_t sorting_layer = 5;
    int32_t sorting_order = 0;
};

struct ChestDefinition {
    std::string id;
    int level = 0;
    std::vector<ItemVisual> visuals;
    engine::Vec2 collider_half_extents{0.5f, 0.35f};
    engine::Vec2 collider_offset{0.0f, 0.35f};
    float open_time = 1.0f;
    std::string loot_coin;
    int loot_count = 0;
    std::string appointed_weapon;
    int weapon_drop_level = -1;
    int weapon_count = 1;
    bool has_coin = false;
    bool material_chest = false;
    bool boss = false;
    bool big = false;
    std::vector<std::string> object_list;
};

struct WeaponDropEntry {
    std::string weapon;
    int weight = 0;
    std::string title;
};

struct WeaponDropLevel {
    int level = 0;
    std::vector<WeaponDropEntry> entries;
};

struct CoinDefinition {
    std::string id;
    int type = 0;
    int value = 0;
    float speed = 20.0f;
    float range = 7.0f;
    std::vector<std::string> frames;
};

struct MaterialDefinition {
    std::string id;
    std::string sprite;
    engine::Vec2 offset{0.0f, 0.0f};
    int price = 0;
    int item_type = 3;
};

struct EnergyPickupDefinition {
    std::string id;
    std::string sprite;
    engine::Vec2 offset{0.0f, 0.0f};
    int value = 8;
    float speed = 14.0f;
    float range = 6.0f;
};

struct SellerVisual {
    std::string npc_body;
    std::string npc_shadow;
    std::string pedestal;
    std::string counter;
};

struct PlantStateVisual {
    int state = 0;
    std::string sprite;
    engine::Vec2 offset{0.0f, 0.0f};
    int32_t sorting_layer = 6;
    int32_t sorting_order = 0;
};

struct PlantDefinition {
    std::string id;
    std::string display_name;
    std::string seed_sprite;
    std::vector<PlantStateVisual> states;
    std::string kind;
    std::string item_id;
    int item_value = 0;
    int harvest_count = 1;
    float growth_seconds = 30.0f;
};

struct ShopItemDefinition {
    std::string id;
    std::string item_name;
    std::string kind;
    std::string sprite;
    int price = 0;
    int health = 0;
    int energy = 0;
};

struct SpecialObjectDefinition {
    std::string id;
    std::string role;
    std::string sprite;
    int price = 0;
    int hp = 0;
    int damage = 0;
    int statue_index = 0;
    int mineral_count = 0;
    std::string drop_item;
};

struct PortalDefinition {
    std::string id;
    std::vector<std::string> frames;
    float fps = 12.0f;
    float duration = 0.5f;
    bool loop = true;
};

// Owns the actual chest00/RGCoin sprite atlases and definitions. The pack is
// loaded once and appended to each room SceneAsset so every room can resolve the
// same recovered sprites through its existing SpriteBatch texture ids.
class ItemAssets {
public:
    bool Load(const platform::FileSystem& filesystem,
              const std::filesystem::path& path, std::string* error);
    bool IsLoaded() const { return loaded_; }

    const engine::assets::SceneAsset& Assets() const { return assets_; }
    const std::vector<ChestDefinition>& Chests() const { return chests_; }
    const std::vector<CoinDefinition>& Coins() const { return coins_; }
    const ChestDefinition* FindChest(const std::string& id) const;
    const CoinDefinition* FindCoin(const std::string& id) const;
    const PortalDefinition* FindPortal(const std::string& id) const;
    const std::vector<MaterialDefinition>& Materials() const {
        return materials_;
    }
    const std::vector<PlantDefinition>& Plants() const { return plants_; }
    const SellerVisual& SellerVisuals() const { return seller_visual_; }
    const PlantDefinition* FindPlant(const std::string& id) const;
    const MaterialDefinition* FindMaterial(const std::string& id) const;
    const std::vector<ShopItemDefinition>& ShopItems() const {
        return shop_items_;
    }
    const ShopItemDefinition* FindShopItem(const std::string& id) const;
    const std::vector<SpecialObjectDefinition>& SpecialObjects() const {
        return special_objects_;
    }
    const SpecialObjectDefinition* FindSpecialObject(
        const std::string& id) const;
    const EnergyPickupDefinition* EnergyPickup() const {
        return energy_loaded_ ? &energy_ : nullptr;
    }
    const std::vector<WeaponDropLevel>& WeaponDrops() const {
        return weapon_drops_;
    }

    // Appends textures/sprites and remaps texture ids. Definitions keep using
    // symbolic sprite ids because each room has independent sprite indices.
    void AppendToScene(engine::assets::SceneAsset& scene) const;

private:
    bool loaded_ = false;
    engine::assets::SceneAsset assets_;
    std::vector<ChestDefinition> chests_;
    std::vector<CoinDefinition> coins_;
    std::vector<PortalDefinition> portals_;
    std::vector<MaterialDefinition> materials_;
    std::vector<PlantDefinition> plants_;
    SellerVisual seller_visual_;
    std::vector<ShopItemDefinition> shop_items_;
    std::vector<SpecialObjectDefinition> special_objects_;
    EnergyPickupDefinition energy_;
    bool energy_loaded_ = false;
    std::vector<WeaponDropLevel> weapon_drops_;
};

} // namespace soulknight::game::items
