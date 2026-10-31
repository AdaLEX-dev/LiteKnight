// Soul Knight Linux — room door lifecycle controller
#include "RoomDoorController.hpp"

#include <utility>

namespace soulknight::game::room {

bool RoomDoorController::Configure(const engine::assets::SceneAsset& scene,
                                   std::string* error) {
    doors_.clear();
    configured_ = false;
    target_open_ = true;
    if (error != nullptr) error->clear();

    for (const engine::assets::SceneDoor& data : scene.doors) {
        Door door(data);
        if (data.enabled && !data.open_animation.IsValid()) {
            if (error != nullptr) {
                *error = "enabled door has no open animation: " + data.id;
            }
            return false;
        }
        if (data.enabled && !data.close_animation.IsValid()) {
            if (error != nullptr) {
                *error = "enabled door has no close animation: " + data.id;
            }
            return false;
        }
        doors_.push_back(std::move(door));
    }
    target_open_ = !scene.doors.empty() && scene.doors.front().open;
    for (const engine::assets::SceneDoor& data : scene.doors) {
        if (data.enabled) {
            target_open_ = data.open;
            break;
        }
    }
    configured_ = true;
    return true;
}

void RoomDoorController::OpenAll() {
    target_open_ = true;
    for (Door& door : doors_) door.Open();
}

void RoomDoorController::CloseAll() {
    target_open_ = false;
    for (Door& door : doors_) door.Close();
}

void RoomDoorController::ToggleAll() {
    if (target_open_) {
        CloseAll();
    } else {
        OpenAll();
    }
}

bool RoomDoorController::Update(float delta_seconds) {
    bool collision_changed = false;
    for (Door& door : doors_) {
        collision_changed = door.Update(delta_seconds) || collision_changed;
    }
    return collision_changed;
}

void RoomDoorController::AddColliders(
    engine::collision::AabbCollisionWorld& world) const {
    // NATIVE APPROXIMATION: the recovered clips animate BoxCollider2D.m_Size
    // (E/W thickness 0.2 -> 1, N/S m_Size.y held at 1) while m_IsTrigger is
    // false. The native world uses the full serialized barrier for the whole
    // non-trigger interval because the transient thin collider still spans the
    // doorway line. m_IsTrigger timing is reproduced exactly from the clips.
    for (const Door& door : doors_) {
        if (!door.IsBlocking()) continue;
        const engine::assets::SceneDoor& data = door.Data();
        const engine::collision::Aabb barrier{
            data.barrier.min, data.barrier.max
        };
        engine::collision::OneWayDirection direction =
            engine::collision::OneWayDirection::PositiveX;
        bool one_way = true;
        switch (data.one_way_block) {
            case engine::assets::DoorOneWayBlock::PositiveX:
                direction = engine::collision::OneWayDirection::PositiveX;
                break;
            case engine::assets::DoorOneWayBlock::NegativeX:
                direction = engine::collision::OneWayDirection::NegativeX;
                break;
            case engine::assets::DoorOneWayBlock::PositiveY:
                direction = engine::collision::OneWayDirection::PositiveY;
                break;
            case engine::assets::DoorOneWayBlock::NegativeY:
                direction = engine::collision::OneWayDirection::NegativeY;
                break;
            case engine::assets::DoorOneWayBlock::None:
                one_way = false;
                break;
        }
        if (one_way) {
            world.AddOneWaySolid(barrier, direction);
        } else {
            world.AddSolid(barrier);
        }
    }
}

} // namespace soulknight::game::room
