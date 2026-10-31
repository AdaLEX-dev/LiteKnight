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
