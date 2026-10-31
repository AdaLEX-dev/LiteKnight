// Soul Knight Linux — room identity/connection foundation
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace soulknight::game::room {

struct Room {
    std::string id;
    std::string scene_path;
};

struct RoomConnection {
    std::string from;
    std::string to;
    std::string kind = "door";
    // Physical door identifiers in the source/destination room. Empty means
    // the connection is logical-only (for example the Living Room hub marker).
    std::string from_door;
    std::string to_door;
};

struct RoomTransition {
    std::string from;
    std::string to;
    std::uint64_t sequence = 0;
};

// Metadata-only room graph. The native two-room transition loads actual scene
// assets; this manager records room identity and allowed connections without
// inventing procedural dungeon rules.
class RoomManager {
public:
    void Configure(std::vector<Room> rooms,
                   std::vector<RoomConnection> connections);
    const Room* Current() const;
    const Room* Find(const std::string& id) const;
    bool CanTransition(const std::string& to) const;
    bool TransitionTo(const std::string& to);
    // Physical transitions select the connection owned by the door the player
    // used. On success `destination_door` receives the matching door id in the
    // destination room (empty for logical connections).
    const RoomConnection* ConnectionFromDoor(const std::string& door_id) const;
    bool TransitionThroughDoor(const std::string& door_id,
                               std::string* destination_door = nullptr);
    bool SetCurrent(const std::string& id);
    const std::vector<Room>& Rooms() const { return rooms_; }
    const std::vector<RoomConnection>& Connections() const { return connections_; }
    const RoomTransition& LastTransition() const { return last_; }
    std::size_t TransitionCount() const { return count_; }

private:
    std::vector<Room> rooms_;
    std::vector<RoomConnection> connections_;
    std::size_t current_ = 0;
    std::size_t count_ = 0;
    RoomTransition last_{};
};

} // namespace soulknight::game::room
