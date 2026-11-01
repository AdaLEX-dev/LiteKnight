// Soul Knight Linux — Application lifecycle
#pragma once

#include "AppConfig.hpp"
#include "platform/wayland/Window.hpp"
#include "platform/timing/FrameTimer.hpp"

#include <chrono>
#include "platform/filesystem/FileSystem.hpp"
#include "platform/input/InputManager.hpp"
#include "platform/input/MovementInput.hpp"
#include "platform/audio/AudioEngine.hpp"
#include "engine/animation/LocomotionAnimator.hpp"
#include "engine/assets/SceneAsset.hpp"
#include "engine/collision/AabbCollision.hpp"
#include "engine/locomotion/PlayerLocomotion.hpp"
#include "engine/locomotion/FacingDirection.hpp"
#include "engine/assets/TextureData.hpp"
#include "engine/rendering/Camera2D.hpp"
#include "engine/rendering/LogicalViewport.hpp"
#include "engine/rendering/PlayerRenderSync.hpp"
#include "engine/rendering/SpriteBatch.hpp"
#include "engine/rendering/SpriteRenderer.hpp"
#include "game/combat/Damage.hpp"
#include "game/combat/ProjectileSystem.hpp"
#include "game/combat/Weapon.hpp"
#include "game/enemy/EnemySystem.hpp"
#include "game/player/PlayerCharacterSet.hpp"
#include "game/test/InteractiveReplay.hpp"
#include "game/test/InteractiveTest.hpp"
#include "game/ui/AssetLab.hpp"
#include "game/ui/DevOverlay.hpp"
#include "game/ui/GameUI.hpp"
#include "game/ui/UIAssets.hpp"
#include "game/ui/UIRenderer.hpp"
#include "game/hub/PlantSystem.hpp"
#include "game/hub/TrainingStake.hpp"
#include "game/hub/Workshop.hpp"
#include "game/pet/PetSystem.hpp"
#include "game/summon/SummonSystem.hpp"
#include "game/player/Ability.hpp"
#include "game/items/ItemAssets.hpp"
#include "game/items/ItemWorld.hpp"
#include "game/items/BoxSystem.hpp"
#include "game/player/Aim.hpp"
#include "game/player/Energy.hpp"
#include "game/save/SaveManager.hpp"
#include "game/weapons/WeaponInventory.hpp"
#include "game/weapons/WeaponPickup.hpp"
#include "game/weapons/WeaponCatalogue.hpp"
#include "game/weapons/WeaponRegistry.hpp"
#include "game/room/ObstacleField.hpp"
#include "game/room/RoomDoorController.hpp"
#include "game/room/DungeonPlan.hpp"
#include "game/room/BiomeSystem.hpp"
#include "game/room/LevelSurface.hpp"
#include "game/room/RoomManager.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    void HandleSceneKeyDown(SDL_Scancode scancode, SDL_Keymod modifiers);
    void HandleUIKey(SDL_Scancode scancode, SDL_Keymod modifiers);
    void ProcessUIAction();
    void DrawGameUI();
    void LoadUiLayout();
    void SaveUiLayout();
    void EnterUiDesignMode();
    void ExitUiDesignMode();
    bool HandleUiDesignEvent(const SDL_Event& event);
    void HandleMainMenuPointer(const SDL_Event& event);
    void LogViewportContract();
    void ConfigureReviewMode();
    void TeleportForReview(const engine::Vec2& position);
    bool LoadWorldRooms();
    bool AdvanceDungeonLevel();
    void ConfigureWorldGraph();
    bool SwitchRoom(const std::string& door_id = {});
    bool ActivateRoom(const std::string& room_id,
                      const std::string& entrance_door);
    void CaptureCurrentRoomState();
    void RestoreCurrentRoomState();
    void UpdateRoomClear();
    void BeginRoomTitle(float seconds);
    void SealUnconnectedDoors();
    bool NearOpenDoor() const;
    const engine::assets::SceneDoor* NearbyOpenDoor() const;
    engine::Vec2 DoorEntrancePosition(
        const engine::assets::SceneAsset& scene,
        const std::string& door_id) const;
    std::string FirstConnectedDoor() const;
    void DrawCollisionDebug();
    void FixedUpdate(double dt);
    void ExerciseWindowIfRequested();
    void BuildRenderBatch();
    void RefreshLevelSurfaces();
    void BuildSyntheticBatch();
    static void AssignUniqueTextureIds(engine::assets::SceneAsset& scene,
                                       uint32_t base);
    void RebuildCollisionWorld();
    void ToggleAllDoors();
    void CloseAllDoors();
    void UpdateDoorTriggers();
    void SwitchCharacter(std::size_t index);
    void CycleCharacter(int delta);
    void CycleObstacleVariant();
    void ResetCombat();
    void UpdateAimInput(const platform::InputSnapshot* snapshot = nullptr);
    void ConfigureActiveWeapon();
    void SwitchWeaponSlot(std::size_t slot);
    void ApplyActiveCharacterStats();
    void ConfigureSecondaryWeapon();
    void ConfigureTraderOffers();
    void RunGameplaySmokeStep();
    void PopulateBiomeEnemies();
    void ShowDialogue(const std::string& speaker, const std::string& text,
                      float seconds = 2.5f);
    void UpdateTrainingStake(float delta_seconds, int melee_damage);
    void BuyTraderItem();
    void ConfigureAbilities();
    void ActivateKnightPower();
    engine::Vec2 AimOrigin() const;
    const soulknight::game::enemy::EnemyInstance* FindAutoAimTarget() const;
    void UpdateAutoAim();
    float AimAngleDegrees() const;
    void SpawnWeaponPickup(std::size_t weapon_index);
    void SpawnWeaponPickupAt(std::size_t weapon_index,
                             const engine::Vec2& position);
    bool TryPickupWeapon();
    bool TryOpenChest();
    void TryInteract();
    void ConfigureRoomItems();
    const engine::assets::InteractionMarker* NearbyInteraction() const;
    void SaveGameToDisk();
    void RefreshSaveSummary();
    void PlayUiSound(const std::string& id);
    void PlayEffect(const std::string& id, float volume = 1.0f);
    void PlayMusic(const std::string& id);
    void UpdateAudio();
    void LoadGameFromDisk();
    void SpawnSelectedEnemy(int count);
    void CycleAssetLabCategory(int delta);
    void CycleAssetLabItem(int delta);
    void ActivateAssetLabSelection();
    void StartInteractiveTest();
    bool LoadReplayIfRequested();
    void StartReplaySession();
    bool ApplyReplayInputForNextTick();
    void ApplyInputSnapshot(const platform::InputSnapshot& snapshot);
    void RecordInteractiveTestTick();
    void FinalizeInteractiveTest(const std::string& result,
                                 const std::string& notes);
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
    soulknight::game::room::RoomManager room_manager_;
    soulknight::game::room::ObstacleField obstacles_;
    soulknight::game::player::PlayerCharacterSet player_characters_;
    soulknight::game::weapons::WeaponRegistry weapons_;
    soulknight::game::weapons::WeaponCatalogue weapon_catalogue_;
    soulknight::game::weapons::WeaponInventory weapon_inventory_;
    soulknight::game::weapons::WeaponPickupSystem weapon_pickups_;
    std::unique_ptr<soulknight::game::save::SaveManager> save_manager_;
    std::string save_slot_summary_;
    platform::AudioEngine audio_engine_;
    bool audio_available_ = false;
    std::vector<platform::AudioEngine::SoundHandle> audio_voices_;
    platform::AudioEngine::SoundHandle music_;
    std::string active_music_;
    soulknight::game::ui::DevOverlay dev_ui_;
    soulknight::game::ui::GameUI game_ui_;
    soulknight::game::ui::HudLayout hud_layout_;
    soulknight::game::ui::UIRenderer ui_renderer_;
    soulknight::game::ui::UIAssets ui_assets_;
    soulknight::game::ui::AssetLab asset_lab_;
    soulknight::game::test::InteractiveTest interactive_test_;
    std::unique_ptr<soulknight::game::test::InteractiveReplay> replay_;
    platform::InputSnapshot replay_snapshot_;
    soulknight::game::combat::Weapon weapon_;
    soulknight::game::combat::Weapon secondary_weapon_;
    soulknight::game::player::AbilityController abilities_;
    soulknight::game::player::AimMode aim_mode_ = soulknight::game::player::AimMode::Auto;
    soulknight::game::combat::ProjectileSystem projectiles_;
    soulknight::game::enemy::EnemySystem enemies_;
    soulknight::game::combat::HealthState player_health_;
    soulknight::game::player::EnergyState player_energy_;
    soulknight::game::items::ItemAssets item_assets_;
    soulknight::game::items::ItemWorld item_world_;
    soulknight::game::items::BoxSystem boxes_;
    soulknight::game::hub::PlantSystem plant_system_;
    soulknight::game::hub::TrainingStake training_stake_;
    soulknight::game::hub::Workshop workshop_;
    std::string workshop_status_;
    soulknight::game::pet::PetSystem pet_;
    std::string active_pet_id_;
    soulknight::game::summon::SummonSystem summons_;
    float stealth_timer_ = 0.0f;
    float attack_speed_timer_ = 0.0f;
    float attack_speed_multiplier_ = 1.0f;
    float camera_flash_timer_ = 0.0f;
    bool revive_used_ = false;
    struct BlackHoleState {
        engine::Vec2 position{0.0f, 0.0f};
        float age = 0.0f;
        float duration = 0.0f;
        float radius = 0.0f;
        int damage = 0;
        bool active = true;
    };
    std::vector<BlackHoleState> black_holes_;
    engine::Vec2 aim_direction_{1.0f, 0.0f};
    engine::Vec2 previous_aim_direction_{1.0f, 0.0f};
    engine::Vec2 render_aim_direction_{1.0f, 0.0f};
    engine::Vec2 previous_player_position_{0.0f, 0.0f};
    engine::Vec2 last_mouse_logical_{0.0f, 0.0f};
    engine::Vec2 last_mouse_world_{0.0f, 0.0f};
    engine::Vec2 auto_aim_target_{0.0f, 0.0f};
    bool auto_aim_has_target_ = false;
    engine::Vec2 render_player_position_{0.0f, 0.0f};
    engine::Vec2 previous_camera_position_{0.0f, 0.0f};
    engine::Camera2D render_camera_;
    engine::rendering::LogicalViewport logical_viewport_;
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
    std::vector<soulknight::game::room::SurfaceInstance> level_surfaces_;
    std::string level_surface_key_;
    bool scene_mode_ = false;
    struct StoredRoom {
        std::string id;
        soulknight::engine::assets::SceneAsset scene;
    };
    std::vector<StoredRoom> stored_rooms_;
    soulknight::game::room::DungeonTemplateSet world_templates_;
    soulknight::game::room::BiomeSystem biome_;
    std::string world_hub_id_ = "hero_room";
    std::string world_hub_path_ = "scenes/hero_room/scene.json";
    bool multi_room_world_ = false;
    std::int32_t map_seed_ = 0;
    std::string room_graph_text_ = "hero_room <-> r1_1_visual";
    std::string world_start_room_id_;
    std::unordered_map<std::string, std::string> room_stage_labels_;
    std::unordered_map<std::string,
        std::vector<soulknight::game::enemy::EnemyInstance>>
        room_enemy_snapshots_;
    std::unordered_set<std::string> cleared_rooms_;
    std::unordered_set<std::string> chest_spawned_rooms_;
    std::string current_chest_definition_;
    std::vector<std::string> visited_rooms_;
    int player_coins_ = 0;
    int player_orbs_ = 0;
    std::vector<soulknight::game::ui::TraderOfferView> trader_offers_;
    bool ui_design_mode_ = false;
    bool ui_design_from_main_menu_ = false;
    bool review_mode_ = false;
    double return_portal_confirm_seconds_ = -1.0;
    int gameplay_smoke_stage_ = 0;
    double gameplay_smoke_stage_time_ = 0.0;
    std::string gameplay_smoke_level_before_;
    float room_title_timer_ = 0.0f;
    float room_clear_timer_ = 0.0f;
    float room_transition_flash_ = 0.0f;
    std::string dialogue_speaker_;
    std::string dialogue_text_;
    float dialogue_timer_ = 0.0f;
    int stake_damage_ = 0;
    engine::Vec2 stake_damage_screen_{0.0f, 0.0f};
    float stake_damage_timer_ = 0.0f;
    unsigned int npc_talk_counter_ = 0;
    std::chrono::steady_clock::time_point last_present_time_{};
    double present_window_sum_ms_ = 0.0;
    double present_window_min_ms_ = 0.0;
    double present_window_max_ms_ = 0.0;
    int present_window_samples_ = 0;
    double present_run_sum_ms_ = 0.0;
    long long present_run_samples_ = 0;
    bool ui_design_dragging_ = false;
    std::string ui_design_selected_;
    engine::Vec2 ui_design_drag_offset_{0.0f, 0.0f};
    engine::Vec2 ui_design_mouse_{0.0f, 0.0f};
    bool main_menu_started_ = false;
    bool collision_debug_ = false;
    bool room_clear_pending_ = false;
    float hit_flash_timer_ = 0.0f;
    int last_framebuffer_width_ = -1;
    int last_framebuffer_height_ = -1;
    int last_window_width_ = -1;
    int last_window_height_ = -1;
    int exit_code_ = 0;
    std::filesystem::path project_root_;
    std::string scene_sha256_;
    std::string reference_apk_sha256_;
    std::string asset_inventory_sha256_;
    std::uint32_t texture_generation_ = 0;

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
