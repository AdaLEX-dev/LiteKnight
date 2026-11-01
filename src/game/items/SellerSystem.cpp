// Soul Knight Linux — recovered dungeon seller stock and purchase state
#include "SellerSystem.hpp"

#include "engine/math/UnityRandom.hpp"
#include "game/weapons/WeaponCatalogue.hpp"

#include <algorithm>
#include <cmath>

namespace soulknight::game::items {
namespace {

SellerOffer MakeOffer(const ItemAssets& assets, const std::string& id,
                      const engine::Vec2& position) {
    SellerOffer offer;
    offer.item_id = id;
    offer.position = position;
    const ShopItemDefinition* item = assets.FindShopItem(id);
    if (item != nullptr) {
        offer.display_name = item->item_name;
        offer.sprite = item->sprite;
        offer.kind = item->kind;
        offer.price = item->price;
        offer.health = item->health;
        offer.energy = item->energy;
    }
    return offer;
}

} // namespace

void SellerSystem::Reset() {
    offers_.clear();
    configured_ = false;
}

bool SellerSystem::Configure(
        const ItemAssets& assets, const engine::assets::SceneAsset& scene,
        int chest_level, int map_seed,
        const weapons::WeaponCatalogue* catalogue,
        const std::string& room_id) {
    Reset();
    if (room_id != "r_sell_visual" || !assets.IsLoaded()) return false;
    // Recovered sell2-1 stock: health_pot, energy_pot, one read_xml=1 weapon
    // container. Container transforms are Odin data; these positions are the
    // native reconstruction on the recovered 19x13 room floor.
    const engine::Vec2 base = scene.spawn;
    offers_.push_back(MakeOffer(assets, "health_pot",
                                base + engine::Vec2{-3.2f, 2.2f}));
    offers_.push_back(MakeOffer(assets, "energy_pot",
                                base + engine::Vec2{0.0f, 2.8f}));

    int weapon_level = std::max(5, chest_level);
    if (weapon_level > 6) weapon_level = 6;
    const WeaponDropLevel* drop = nullptr;
    for (const WeaponDropLevel& candidate : assets.WeaponDrops()) {
        if (candidate.level == weapon_level) {
            drop = &candidate;
            break;
        }
    }
    if (drop != nullptr && !drop->entries.empty()) {
        engine::RGRandomStream random;
        random.SetRandomSeed(static_cast<int32_t>(
            (static_cast<uint32_t>(map_seed) ^ 0x53454C4Cu) & 0x7FFFFFFFu));
        const WeaponDropEntry& entry = drop->entries[
            static_cast<std::size_t>(random.Range(
                0, static_cast<int>(drop->entries.size())))];
        SellerOffer weapon;
        weapon.item_id = entry.weapon;
        weapon.display_name = entry.title;
        weapon.kind = "weapon";
        weapon.weapon_id = entry.weapon;
        weapon.position = base + engine::Vec2{3.2f, 2.2f};
        weapon.price = 30;
        if (catalogue != nullptr) {
            if (const weapons::WeaponCatalogueEntry* info =
                    catalogue->Find(entry.weapon); info != nullptr &&
                info->item_value > 0) {
                weapon.price = info->item_value;
            }
        }
        offers_.push_back(std::move(weapon));
    }
    configured_ = !offers_.empty();
    return configured_;
}

const SellerOffer* SellerSystem::FindNearby(
        const engine::Vec2& player_position, float radius) const {
    const SellerOffer* best = nullptr;
    float best_distance = radius * radius;
    for (const SellerOffer& offer : offers_) {
        if (offer.sold) continue;
        const float distance = (offer.position - player_position).LengthSquared();
        if (distance <= best_distance) {
            best = &offer;
            best_distance = distance;
        }
    }
    return best;
}

const SellerOffer* SellerSystem::FindByIndex(std::size_t index) const {
    return index < offers_.size() ? &offers_[index] : nullptr;
}

SellerPurchase SellerSystem::Purchase(
        std::size_t index, int& coins, int& player_hp, int player_max_hp,
        int& player_energy, int player_max_energy) {
    if (index >= offers_.size()) return SellerPurchase::Invalid;
    SellerOffer& offer = offers_[index];
    if (offer.sold) return SellerPurchase::Sold;
    if (coins < offer.price) return SellerPurchase::NoMoney;
    coins -= offer.price;
    player_hp = std::min(player_max_hp, player_hp + offer.health);
    player_energy = std::min(player_max_energy, player_energy + offer.energy);
    offer.sold = true;
    return SellerPurchase::Success;
}

void SellerSystem::RestoreSold(const std::vector<std::string>& sold_ids) {
    for (SellerOffer& offer : offers_) {
        offer.sold = std::find(sold_ids.begin(), sold_ids.end(),
                               offer.item_id) != sold_ids.end();
    }
}

} // namespace soulknight::game::items
