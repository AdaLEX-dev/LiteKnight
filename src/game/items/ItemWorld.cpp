#include "ItemWorld.hpp"

#include "engine/math/UnityRandom.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

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
                          const engine::Vec2& chest_position,
                          const std::string& chest_definition) {
    room_id_ = room_id;
    RoomState& room = rooms_[room_id];
    if (room.configured) return;
    room.configured = true;
    if (spawn_chest) {
        ChestInstance chest;
        chest.definition = chest_definition;
        chest.position = chest_position;
        room.chests.push_back(std::move(chest));
    }
}

void ItemWorld::AddChest(const std::string& definition,
                         const engine::Vec2& position) {
    RoomState& room = Current(room_id_);
    room.configured = true;
    ChestInstance chest;
    chest.definition = definition;
    chest.position = position;
    room.chests.push_back(std::move(chest));
}

void ItemWorld::SpawnCoins(const std::string& definition, int count,
                           const engine::Vec2& position) {
    if (definition.empty() || count <= 0) return;
    RoomState& room = Current(room_id_);
    for (int i = 0; i < count; ++i) {
        CoinInstance spawned;
        spawned.definition = definition;
        const float angle = static_cast<float>(i) * 2.399963f;
        spawned.position = position + engine::Vec2{
            std::cos(angle) * 0.22f, 0.34f + std::sin(angle) * 0.12f};
        spawned.velocity = {std::cos(angle) * 1.8f,
                            std::sin(angle) * 1.8f + 2.0f};
        room.coins.push_back(std::move(spawned));
    }
}

bool ItemWorld::AutoOpenNear(const engine::Vec2& player_position, float radius) {
    RoomState& room = Current(room_id_);
    bool opened = false;
    const float radius_squared = radius * radius;
    for (ChestInstance& chest : room.chests) {
        if (chest.state != ChestState::Closed) continue;
        if ((chest.position - player_position).LengthSquared() > radius_squared) {
            continue;
        }
        chest.state = ChestState::Opening;
        chest.timer = 0.0f;
        opened = true;
    }
    return opened;
}

void ItemWorld::Update(float delta_seconds, const engine::Vec2& player_position,
                       const ItemAssets& assets) {
    if (delta_seconds <= 0.0f) return;
    RoomState& room = Current(room_id_);
    for (std::size_t chest_index = 0; chest_index < room.chests.size();
         ++chest_index) {
        ChestInstance& chest = room.chests[chest_index];
        if (chest.state != ChestState::Opening) continue;
        chest.timer += delta_seconds;
        const ChestDefinition* definition = assets.FindChest(chest.definition);
        const float open_time = definition != nullptr
            ? std::max(0.01f, definition->open_time) : 1.0f;
        if (chest.timer < open_time) continue;
        chest.state = ChestState::Open;
        if (chest.loot_spawned) continue;
        chest.loot_spawned = true;
        if (definition == nullptr) continue;
        std::uint32_t hash = 2166136261u;
        for (char c : room_id_) {
            hash ^= static_cast<unsigned char>(c);
            hash *= 16777619u;
        }
        hash ^= static_cast<std::uint32_t>(chest_index) * 0x9E3779B9u;
        engine::RGRandomStream random;
        random.SetRandomSeed(static_cast<int32_t>(hash & 0x7FFFFFFFu));
        const auto roll_weapon = [&](int drop_level) {
            const WeaponDropLevel* drop = nullptr;
            for (const WeaponDropLevel& candidate : assets.WeaponDrops()) {
                if (candidate.level == drop_level) {
                    drop = &candidate;
                    break;
                }
            }
            if (drop == nullptr) return;
            std::vector<const WeaponDropEntry*> candidates;
            int total_weight = 0;
            for (const WeaponDropEntry& entry : drop->entries) {
                if (!available_weapons_.empty() &&
                    std::find(available_weapons_.begin(),
                              available_weapons_.end(),
                              entry.weapon) == available_weapons_.end()) {
                    continue;
                }
                candidates.push_back(&entry);
                total_weight += entry.weight;
            }
            if (candidates.empty() || total_weight <= 0) return;
            int roll = random.Range(0, total_weight);
            for (const WeaponDropEntry* entry : candidates) {
                if (roll < entry->weight) {
                    chest.pending_weapons.push_back(entry->weapon);
                    return;
                }
                roll -= entry->weight;
            }
        };

        // RGChestAppoint has a fixed object_list entry.
        if (!definition->appointed_weapon.empty()) {
            chest.pending_weapons.push_back(definition->appointed_weapon);
        } else if (definition->boss) {
            // RGChestBoss.Start: finalLevel = max(5, level + chest_level).
            // OpenChest: 25% fixed boss weapon, otherwise one sample weapon.
            const int final_level = std::max(
                5, definition->level + chest_level_);
            if (!definition->object_list.empty() &&
                random.Range(0, 100) <= 24) {
                const int index = random.Range(
                    0, static_cast<int>(definition->object_list.size()));
                chest.pending_weapons.push_back(
                    definition->object_list[static_cast<std::size_t>(index)]);
            } else {
                roll_weapon(final_level);
            }
        } else if (definition->big) {
            // RGChestBig.Start: finalLevel = level + chest_level. Each item has
            // a 50% chance to be one drop level lower (clamped 0..5).
            const int final_level = definition->level + chest_level_;
            for (int i = 0; i < definition->weapon_count; ++i) {
                const int reduce = random.Range(0, 100) < 50 ? 1 : 0;
                roll_weapon(std::clamp(final_level - reduce, 0, 5));
            }
        } else if (definition->weapon_drop_level >= -2) {
            // RGChest.Start: level >= 6 clamps to 6; otherwise
            // max(5, level + MapManagerLevel.chest_level).
            int drop_level = definition->weapon_drop_level;
            if (drop_level == -2) {
                drop_level = chest_level_;
            } else {
                drop_level = definition->level >= 6
                    ? 6 : std::max(5, definition->level + chest_level_);
            }
            for (int i = 0; i < definition->weapon_count; ++i) {
                roll_weapon(drop_level);
            }
        }
        const CoinDefinition* coin = assets.FindCoin(definition->loot_coin);
        if (coin == nullptr) continue;
        const int count = std::max(0, definition->loot_count);
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

void ItemWorld::SetAvailableWeapons(std::vector<std::string> weapon_ids) {
    available_weapons_ = std::move(weapon_ids);
}

std::vector<WeaponLootDrop> ItemWorld::TakePendingWeaponDrops() {
    std::vector<WeaponLootDrop> drops;
    RoomState& room = Current(room_id_);
    for (ChestInstance& chest : room.chests) {
        for (const std::string& weapon : chest.pending_weapons) {
            drops.push_back({weapon, chest.position});
        }
        chest.pending_weapons.clear();
        chest.pending_weapon.clear();
    }
    return drops;
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

std::vector<RoomItemSnapshot> ItemWorld::SnapshotRooms() const {
    std::vector<RoomItemSnapshot> snapshots;
    for (const auto& [id, room] : rooms_) {
        RoomItemSnapshot snapshot;
        snapshot.id = id;
        snapshot.chests = room.chests;
        snapshot.coins = room.coins;
        snapshots.push_back(std::move(snapshot));
    }
    std::sort(snapshots.begin(), snapshots.end(),
              [](const RoomItemSnapshot& a, const RoomItemSnapshot& b) {
                  return a.id < b.id;
              });
    return snapshots;
}

void ItemWorld::RestoreRooms(const std::vector<RoomItemSnapshot>& rooms) {
    rooms_.clear();
    for (const RoomItemSnapshot& snapshot : rooms) {
        RoomState state;
        state.configured = true;
        state.chests = snapshot.chests;
        state.coins = snapshot.coins;
        rooms_[snapshot.id] = std::move(state);
    }
}

const std::vector<CoinInstance>& ItemWorld::Coins() const {
    static const std::vector<CoinInstance> empty;
    const auto it = rooms_.find(room_id_);
    return it == rooms_.end() ? empty : it->second.coins;
}

} // namespace soulknight::game::items
