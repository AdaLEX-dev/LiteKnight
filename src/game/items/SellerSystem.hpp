// Soul Knight Linux — recovered dungeon seller stock and purchase state
#pragma once

#include "ItemAssets.hpp"
#include "engine/assets/SceneAsset.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace soulknight::game::weapons { class WeaponCatalogue; }

namespace soulknight::game::items {

struct SellerOffer {
    std::string item_id;
    std::string display_name;
    std::string sprite;
    std::string kind;
    std::string weapon_id;
    engine::Vec2 position{0.0f, 0.0f};
    int price = 0;
    int health = 0;
    int energy = 0;
    bool sold = false;
};

enum class SellerPurchase { Success, NoMoney, Sold, Invalid };

// Native seller for the recovered r_sell room. Stock is the exact recovered
// sell2-1 layout (health pot, energy pot, restore pot, one weapon container);
// the world positions are a native reconstruction because the RandomObject
// container transforms are Odin-serialized.
class SellerSystem {
public:
    void Reset();
    bool Configure(const ItemAssets& assets,
                   const engine::assets::SceneAsset& scene,
                   int chest_level, int map_seed,
                   const weapons::WeaponCatalogue* catalogue,
                   const std::string& room_id);
    bool Configured() const { return configured_; }

    const std::vector<SellerOffer>& Offers() const { return offers_; }
    std::vector<SellerOffer>& OffersMutable() { return offers_; }
    const SellerOffer* FindNearby(const engine::Vec2& player_position,
                                  float radius) const;
    const SellerOffer* FindByIndex(std::size_t index) const;

    SellerPurchase Purchase(std::size_t index, int& coins,
                            int& player_hp, int player_max_hp,
                            int& player_energy, int player_max_energy);
    void MarkSold(std::size_t index) { if (index < offers_.size()) offers_[index].sold = true; }
    void RestoreSold(const std::vector<std::string>& sold_ids);

private:
    std::vector<SellerOffer> offers_;
    bool configured_ = false;
};

} // namespace soulknight::game::items
