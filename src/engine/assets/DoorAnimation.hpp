// Soul Knight Linux — recovered door animation data
#pragma once

#include <string>
#include <vector>

namespace soulknight::engine::assets {

struct DoorCurveKey {
    float time = 0.0f;
    float value = 0.0f;
};

// Linear replay of the recovered Unity StreamedClip keyframes. Unity stores
// cubic coefficients alongside each key; the embedded door clips only contain
// sparse keys and the native runtime deliberately replays the serialized key
// values linearly. See analysis/re/room_topology.md for the source curves.
struct DoorCurve {
    std::vector<DoorCurveKey> keys;

    bool Empty() const { return keys.empty(); }
    // Continuous keys (for example the ornament local y) interpolate linearly.
    float Evaluate(float time) const;
    // Unity m_Enabled and m_IsTrigger curves are discrete; the previous key is
    // held until the next serialized key time. Sprite PPtr keys use the same
    // step behavior in DoorAnimationClip::SpriteAt.
    float EvaluateStep(float time) const;
};

struct DoorSpriteKey {
    float time = 0.0f;
    std::string sprite_id;
};

// A single recovered Animator clip. Values are sampled from the serialized
// StreamedClip and PPtr sprite curves instead of hardcoding door behaviour in
// Application.
struct DoorAnimationClip {
    std::string id;
    float duration = 0.0f;
    float sample_rate = 0.0f;
    bool loop = false;

    // Local y of the row ornament child ("dN/d"). Closed = 1, open = 0.1.
    DoorCurve leaf_extent;
    // m_Enabled of the per-leaf shadow sprite renderer.
    DoorCurve shadow_visible;
    // m_Enabled of the per-leaf ornament sprite renderer.
    DoorCurve ornament_visible;
    // BoxCollider2D.m_IsTrigger. 0 means solid, 1 means passable.
    DoorCurve collider_trigger;
    // BoxCollider2D.m_Size components. Exported for provenance and native
    // collision documentation; see RoomDoorController for the native mapping.
    DoorCurve collider_size_x;
    DoorCurve collider_size_y;

    // SpriteRenderer.m_Sprite keys for each leaf. The recovered clips switch
    // door02 <-> door03 at a serialized time.
    std::vector<DoorSpriteKey> leaf_sprites;

    bool IsValid() const { return duration > 0.0f; }
    float SampleTime(float time) const;
    const std::string& SpriteAt(float time) const;
};

} // namespace soulknight::engine::assets
