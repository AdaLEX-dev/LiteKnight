// Soul Knight Linux — room door lifecycle controller
#pragma once

#include "Door.hpp"
#include "engine/assets/SceneAsset.hpp"
#include "engine/collision/AabbCollision.hpp"

#include <string>
#include <vector>

namespace soulknight::game::room {

// Owns the four recovered room doors. The controller is deliberately separate
// from Application: it exposes logical open/close requests, deterministic
// animation updates, collision feeds and render state.
class RoomDoorController {
public:
    RoomDoorController() = default;

    bool Configure(const engine::assets::SceneAsset& scene, std::string* error);
    bool Configured() const { return configured_; }

    void OpenAll();
    void CloseAll();
    void ToggleAll();
    // A sealed door has no matching map edge. It stays closed and is ignored by
    // OpenAll so the physical room boundary always agrees with the map graph.
    void SealDoor(std::size_t index);
    bool IsSealed(std::size_t index) const;
    bool TargetOpen() const { return target_open_; }

    // Returns true when any door's blocking state changed and the caller must
    // rebuild its collision world.
    bool Update(float delta_seconds);

    void AddColliders(engine::collision::AabbCollisionWorld& world) const;

    std::size_t Count() const { return doors_.size(); }
    const Door& At(std::size_t index) const { return doors_[index]; }
    Door& At(std::size_t index) { return doors_[index]; }
    const std::vector<Door>& Doors() const { return doors_; }

private:
    std::vector<Door> doors_;
    std::vector<bool> sealed_;
    bool configured_ = false;
    bool target_open_ = true;
};

} // namespace soulknight::game::room
