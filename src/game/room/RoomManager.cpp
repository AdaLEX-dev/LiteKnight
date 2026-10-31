#include "RoomManager.hpp"

#include <utility>

namespace soulknight::game::room {

void RoomManager::Configure(std::vector<Room> rooms,
                            std::vector<RoomConnection> connections) {
    rooms_ = std::move(rooms);
    connections_ = std::move(connections);
    current_ = 0;
    count_ = 0;
    last_ = {};
}

const Room* RoomManager::Current() const {
    return current_ < rooms_.size() ? &rooms_[current_] : nullptr;
}

const Room* RoomManager::Find(const std::string& id) const {
    for (const Room& room : rooms_) {
        if (room.id == id) return &room;
    }
    return nullptr;
}

bool RoomManager::CanTransition(const std::string& to) const {
    const Room* current = Current();
    if (current == nullptr) return false;
    for (const RoomConnection& connection : connections_) {
        if (connection.from == current->id && connection.to == to) return true;
        if (connection.from == to && connection.to == current->id &&
            connection.kind == "bidirectional") {
            return true;
        }
    }
    return false;
}

const RoomConnection* RoomManager::ConnectionFromDoor(
    const std::string& door_id) const {
    const Room* current = Current();
    if (current == nullptr || door_id.empty()) return nullptr;
    for (const RoomConnection& connection : connections_) {
        if (connection.from == current->id && connection.from_door == door_id) {
            return &connection;
        }
        if (connection.to == current->id && connection.to_door == door_id &&
            connection.kind == "bidirectional") {
            return &connection;
        }
    }
    return nullptr;
}

bool RoomManager::TransitionThroughDoor(const std::string& door_id,
                                        std::string* destination_door) {
    const Room* current = Current();
    const RoomConnection* connection = ConnectionFromDoor(door_id);
    if (current == nullptr || connection == nullptr) return false;
    const bool forward = connection->from == current->id;
    const std::string& destination =
        forward ? connection->to : connection->from;
    if (!TransitionTo(destination)) return false;
    if (destination_door != nullptr) {
        *destination_door = forward ? connection->to_door
                                    : connection->from_door;
    }
    return true;
}

bool RoomManager::SetCurrent(const std::string& id) {
    for (std::size_t i = 0; i < rooms_.size(); ++i) {
        if (rooms_[i].id == id) {
            current_ = i;
            return true;
        }
    }
    return false;
}

bool RoomManager::TransitionTo(const std::string& to) {
    if (!CanTransition(to)) return false;
    std::size_t next = rooms_.size();
    for (std::size_t i = 0; i < rooms_.size(); ++i) {
        if (rooms_[i].id == to) { next = i; break; }
    }
    if (next == rooms_.size()) return false;
    const Room* current = Current();
    last_.from = current != nullptr ? current->id : std::string{};
    last_.to = to;
    last_.sequence = ++count_;
    current_ = next;
    return true;
}

} // namespace soulknight::game::room
