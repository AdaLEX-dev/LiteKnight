// Soul Knight Linux — recovered asset preview laboratory
#pragma once

#include "engine/assets/SceneAsset.hpp"
#include "engine/math/Vec2.hpp"

#include <cstddef>
#include <string>

namespace soulknight::engine {
struct Camera2D;
}
namespace soulknight::engine::rendering {
class SpriteBatch;
}
namespace soulknight::game::ui {
class DevOverlay;
enum class AssetCategory : int {
    Characters = 0,
    Weapons,
    Projectiles,
    Enemies,
    Obstacles,
    Doors,
    Textures,
    Sprites,
    Animations,
    Effects,
    UI,
    Items,
    Count
};

// Preview/inspection mode for recovered assets. It is intentionally separate
// from gameplay selectors so an asset can be inspected without spawning or
// mutating the live room.
class AssetLab {
public:
    void Open();
    void Close();
    void Toggle();
    bool IsOpen() const { return open_; }

    void NextCategory(int delta);
    void NextItem(int delta);
    void SetItemIndex(std::size_t index) { item_ = index; time_ = 0.0; }
    void TogglePause();
    void StepFrame(int direction);
    void Restart() { time_ = 0.0; paused_ = true; }
    bool Paused() const { return paused_; }
    AssetCategory Category() const { return category_; }
    std::size_t ItemIndex() const { return item_; }

    void Update(float delta_seconds);
    void Draw(engine::rendering::SpriteBatch& batch,
              const engine::assets::SceneAsset& scene,
              const engine::Camera2D& camera,
              const DevOverlay& overlay) const;

    const char* CategoryName() const;
    std::size_t ItemCount(const engine::assets::SceneAsset& scene) const;
    std::string ItemId(const engine::assets::SceneAsset& scene) const;

private:
    const engine::animation::AnimationClip* CurrentClip(
        const engine::assets::SceneAsset& scene) const;
    int32_t CurrentSpriteIndex(const engine::assets::SceneAsset& scene) const;
    float CurrentPPU(const engine::assets::SceneAsset& scene) const;
    engine::Vec2 CurrentPivot(const engine::assets::SceneAsset& scene) const;

    bool open_ = false;
    bool paused_ = false;
    AssetCategory category_ = AssetCategory::Characters;
    std::size_t item_ = 0;
    double time_ = 0.0;
};

} // namespace soulknight::game::ui
