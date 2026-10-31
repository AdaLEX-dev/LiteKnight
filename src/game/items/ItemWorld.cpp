#include "ItemWorld.hpp"

#include <algorithm>
#include <cmath>

namespace soulknight::game::items {

void ItemWorld::Reset() {
    rooms_.clear();
    room_id_.clear();
}

ItemWorld::RoomState& ItemWorld::Current(const std::string& room_id,
                                         const engine::Vec2&) {
    if (room_id_ != room_id) room_id_ = room_id;
    return rooms_[room_id];
}

void ItemWorld::EnterRoom(const std::string& room_id, bool spawn_chest,
                          const engine::Vec2& chest_position) {
    room_id_ = room_id;
    RoomState& room = rooms_[room_id];
    if (room.configured) return;
    room.configured = true;
    if (spawn_chest) {
        ChestInstance chest;
        chest.definition = "chest00";
        chest.position = chest_position;
        room.chests.push_back(std::move(chest));
    }
}

void ItemWorld::Update(float delta_seconds, const engine::Vec2& player_position,
                       const ItemAssets& assets) {
    if (delta_seconds <= 0.0f) return;
    RoomState& room = Current(room_id_);
    for (ChestInstance& chest : room.chests) {
        if (chest.state != ChestState::Opening) continue;
        chest.timer += delta_seconds;
        if (chest.timer < 0.45f) continue;
        chest.state = ChestState::Open;
        if (chest.loot_spawned) continue;
        chest.loot_spawned = true;
        const ChestDefinition* definition = assets.FindChest(chest.definition);
        if (definition == nullptr) continue;
        const CoinDefinition* coin = assets.FindCoin(definition->loot_coin);
        if (coin == nullptr) continue;
        const int count = std::max(1, definition->loot_count);
        for (int i = 0; i < count; ++i) {
            CoinInstance spawned;
            spawned.definition = coin->id;
            const float angle = static_cast<float>(i) * 2.399963f;
            spawned.position = chest.position + engine::Vec2{
                std::cos(angle) * 0.22f, 0.34f + std::sin(angle) * 0.12f};
            spawned.velocity = {std::cos(angle) * 1.8f,
                                std::sin(angle) * 1.8f + 2.0f};
            room.coins.push_back(std::move(spawned));
        }
    }
    for (CoinInstance& coin : room.coins) {
        if (!coin.active) continue;
        coin.age += delta_seconds;
        const CoinDefinition* definition = assets.FindCoin(coin.definition);
        if (definition == nullptr) continue;
        const engine::Vec2 delta = player_position - coin.position;
        const float distance = delta.Length();
        if (distance < definition->range && distance > 0.01f) {
            coin.velocity += delta.Normalized() *
                             (definition->speed * 0.55f * delta_seconds);
            coin.velocity *= std::max(0.0f, 1.0f - 3.0f * delta_seconds);
        } else {
            coin.velocity.y -= 2.5f * delta_seconds;
            coin.velocity.x *= std::max(0.0f, 1.0f - 2.5f * delta_seconds);
        }
        coin.position += coin.velocity * delta_seconds;
    }
}

int ItemWorld::CollectCoins(const engine::Vec2& player_position, float radius,
                            const ItemAssets& assets) {
    RoomState& room = Current(room_id_);
    int collected = 0;
    for (CoinInstance& coin : room.coins) {
        if (!coin.active) continue;
        if ((coin.position - player_position).LengthSquared() > radius * radius) {
            continue;
        }
        const CoinDefinition* definition = assets.FindCoin(coin.definition);
        if (definition == nullptr) continue;
        coin.active = false;
        collected += definition->value;
    }
    return collected;
}

bool ItemWorld::OpenNearestChest(const engine::Vec2& player_position,
                                 float radius) {
    RoomState& room = Current(room_id_);
    for (ChestInstance& chest : room.chests) {
        if (chest.state != ChestState::Closed) continue;
        if ((chest.position - player_position).LengthSquared() >
            radius * radius) {
            continue;
        }
        chest.state = ChestState::Opening;
        chest.timer = 0.0f;
        chest.loot_spawned = false;
        return true;
    }
    return false;
}

const ChestInstance* ItemWorld::NearestChest(
    const engine::Vec2& player_position, float radius) const {
    const auto it = rooms_.find(room_id_);
    if (it == rooms_.end()) return nullptr;
    const ChestInstance* best = nullptr;
    float best_distance = radius * radius;
    for (const ChestInstance& chest : it->second.chests) {
        const float distance = (chest.position - player_position).LengthSquared();
        if (distance <= best_distance) {
            best_distance = distance;
            best = &chest;
        }
    }
    return best;
}

const ChestInstance* ItemWorld::NearestClosedChest(
    const engine::Vec2& player_position, float radius) const {
    const ChestInstance* chest = NearestChest(player_position, radius);
    return chest != nullptr && chest->state == ChestState::Closed ? chest
                                                                  : nullptr;
}

const std::vector<ChestInstance>& ItemWorld::Chests() const {
    static const std::vector<ChestInstance> empty;
    const auto it = rooms_.find(room_id_);
    return it == rooms_.end() ? empty : it->second.chests;
}

const std::vector<CoinInstance>& ItemWorld::Coins() const {
    static const std::vector<CoinInstance> empty;
    const auto it = rooms_.find(room_id_);
    return it == rooms_.end() ? empty : it->second.coins;
}

} // namespace soulknight::game::items
