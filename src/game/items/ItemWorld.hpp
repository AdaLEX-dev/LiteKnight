// Soul Knight Linux — chest/coin world state
#pragma once

#include "ItemAssets.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace soulknight::game::items {

enum class ChestState { Closed, Opening, Open };

struct ChestInstance {
    std::string definition;
    engine::Vec2 position;
    ChestState state = ChestState::Closed;
    float timer = 0.0f;
    bool loot_spawned = false;
    std::string pending_weapon;
};

struct WeaponLootDrop {
    std::string weapon;
    engine::Vec2 position;
};

struct CoinInstance {
    std::string definition;
    engine::Vec2 position;
    engine::Vec2 velocity;
    float age = 0.0f;
    bool active = true;
};

// Session-scoped per-room item state. The save format is still v2; chest/coin
// state deliberately resets deterministically on a new process, while room
// transitions preserve it in memory. See docs/SAVE_SYSTEM.md.
class ItemWorld {
public:
    void Reset();
    void EnterRoom(const std::string& room_id, bool spawn_chest,
                   const engine::Vec2& chest_position,
                   const std::string& chest_definition = "chest00");
    void AddChest(const std::string& definition, const engine::Vec2& position);
    // RGChest opens on trigger/proximity; the reference normal-room silver
    // chest does not require an E button.
    bool AutoOpenNear(const engine::Vec2& player_position, float radius);
    void Update(float delta_seconds, const engine::Vec2& player_position,
                const ItemAssets& assets);
    void SetAvailableWeapons(std::vector<std::string> weapon_ids);
    std::vector<WeaponLootDrop> TakePendingWeaponDrops();
    int CollectCoins(const engine::Vec2& player_position, float radius,
                     const ItemAssets& assets);

    bool OpenNearestChest(const engine::Vec2& player_position, float radius);
    const ChestInstance* NearestChest(const engine::Vec2& player_position,
                                      float radius) const;
    const ChestInstance* NearestClosedChest(const engine::Vec2& player_position,
                                            float radius) const;

    const std::vector<ChestInstance>& Chests() const;
    const std::vector<CoinInstance>& Coins() const;

private:
    struct RoomState {
        bool configured = false;
        std::vector<ChestInstance> chests;
        std::vector<CoinInstance> coins;
    };
    RoomState& Current(const std::string& room_id,
                       const engine::Vec2& fallback_position = {});
    std::string room_id_;
    std::unordered_map<std::string, RoomState> rooms_;
    std::vector<std::string> available_weapons_;
};

} // namespace soulknight::game::items
