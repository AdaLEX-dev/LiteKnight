// Soul Knight Linux — Application lifecycle
#pragma once

#include "AppConfig.hpp"
#include "platform/wayland/Window.hpp"
#include "platform/timing/FrameTimer.hpp"
#include "platform/filesystem/FileSystem.hpp"
#include "platform/input/InputManager.hpp"
#include "platform/input/MovementInput.hpp"
#include "engine/animation/LocomotionAnimator.hpp"
#include "engine/assets/SceneAsset.hpp"
#include "engine/collision/AabbCollision.hpp"
#include "engine/locomotion/PlayerLocomotion.hpp"
#include "engine/locomotion/FacingDirection.hpp"
#include "engine/assets/TextureData.hpp"
#include "engine/rendering/Camera2D.hpp"
#include "engine/rendering/PlayerRenderSync.hpp"
#include "engine/rendering/SpriteBatch.hpp"
#include "engine/rendering/SpriteRenderer.hpp"
#include "game/combat/Damage.hpp"
#include "game/combat/ProjectileSystem.hpp"
#include "game/combat/Weapon.hpp"
#include "game/enemy/EnemySystem.hpp"
#include "game/player/PlayerCharacterSet.hpp"
#include "game/room/ObstacleField.hpp"
#include "game/room/RoomDoorController.hpp"

#include <memory>
#include <string>
#include <vector>

namespace soulknight::engine::rendering {
class VulkanRenderer;
}

namespace soulknight::app {

// Owns the platform/runtime lifecycle. Gameplay must not call SDL or Vulkan
// directly; this is the only object that sequences platform and renderer
// shutdown.
class Application {
public:
    explicit Application(AppConfig config = {});
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Initialize();
    int Run();
    void RequestShutdown();
    void Shutdown();

    bool IsInitialized() const { return initialized_; }
    bool IsRunning() const { return running_; }
    const AppConfig& GetConfig() const { return config_; }

private:
    void PollEvents();
    void FixedUpdate(double dt);
    void ExerciseWindowIfRequested();
    void BuildRenderBatch();
    void BuildSyntheticBatch();
    void RebuildCollisionWorld();
    void ToggleAllDoors();
    void CloseAllDoors();
    void UpdateDoorTriggers();
    void SwitchCharacter(std::size_t index);
    void CycleCharacter(int delta);
    void CycleObstacleVariant();
    void ResetCombat();
    void UpdateAimInput();
    bool SmokeTestExpired() const;

    AppConfig config_;
    platform::Window window_;
    std::unique_ptr<engine::rendering::VulkanRenderer> renderer_;
    engine::rendering::SpriteBatch sprite_batch_;
    platform::FrameTimer frame_timer_;
    platform::FileSystem filesystem_;
    soulknight::engine::assets::TextureData smoke_texture_;
    soulknight::engine::assets::SceneAsset scene_;
    engine::animation::LocomotionAnimator locomotion_animator_;
    engine::locomotion::PlayerLocomotion player_;
    engine::collision::AabbCollisionWorld collision_world_;
    engine::rendering::PlayerRenderBinding player_render_;
    soulknight::game::room::RoomDoorController room_doors_;
    soulknight::game::room::ObstacleField obstacles_;
    soulknight::game::player::PlayerCharacterSet player_characters_;
    soulknight::game::combat::Weapon weapon_;
    soulknight::game::combat::ProjectileSystem projectiles_;
    soulknight::game::enemy::EnemySystem enemies_;
    soulknight::game::combat::HealthState player_health_;
    engine::Vec2 aim_direction_{1.0f, 0.0f};
    bool mouse_aim_valid_ = false;
    engine::locomotion::FacingDirection facing_ =
        engine::locomotion::FacingDirection::Right;
    bool player_dead_ = false;
    bool attack_held_ = false;
    std::vector<bool> door_trigger_inside_;
    platform::InputManager input_manager_;
    platform::MovementInput movement_input_;
    engine::Camera2D camera_;
    std::string last_animation_sprite_;
    engine::animation::LocomotionState last_animation_state_ =
        engine::animation::LocomotionState::Idle;
    int animation_logs_remaining_ = 0;
    std::vector<engine::rendering::TextureUpload> texture_uploads_;
    bool scene_mode_ = false;

    bool sdl_initialized_ = false;
    bool vulkan_library_loaded_ = false;
    bool initialized_ = false;
    bool running_ = false;
    bool shutdown_complete_ = false;

    double smoke_start_seconds_ = -1.0;
    double simulation_time_ = 0.0;
    unsigned long long simulation_ticks_ = 0;
    unsigned long long render_frames_ = 0;

    // Performance sample accumulation, deliberately measured rather than
    // asserted. Warm-up frames include initialization and are excluded.
    int warmup_frames_ = 0;
    int perf_samples_ = 0;
    double perf_total_frame_ms_ = 0.0;
    unsigned long long perf_total_sim_steps_ = 0;

    // Optional window-contract exercise for the manual smoke test.
    int exercise_stage_ = 0;
};

} // namespace soulknight::app
