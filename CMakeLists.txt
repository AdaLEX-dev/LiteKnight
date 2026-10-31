# Soul Knight Linux — CMake Build System
# CMake 3.25+ required (CMake 4 recommended)

cmake_minimum_required(VERSION 3.25)

# VERSION is the single source of truth for the application/version string.
# CMake's project() accepts only numeric x.y.z, so the full semantic version
# (including -dev) is kept separately and exported to the executable.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" LITEKNIGHT_VERSION LIMIT_COUNT 1)
string(STRIP "${LITEKNIGHT_VERSION}" LITEKNIGHT_VERSION)
if(NOT LITEKNIGHT_VERSION)
    set(LITEKNIGHT_VERSION "0.0.0-dev")
endif()
string(REGEX MATCH "^[0-9]+\.[0-9]+\.[0-9]+" LITEKNIGHT_PROJECT_VERSION
       "${LITEKNIGHT_VERSION}")
if(NOT LITEKNIGHT_PROJECT_VERSION)
    set(LITEKNIGHT_PROJECT_VERSION "0.0.0")
endif()

project(liteknight
    VERSION ${LITEKNIGHT_PROJECT_VERSION}
    LANGUAGES CXX
    DESCRIPTION "LiteKnight — native Linux devroom and game reconstruction"
)

# ============================================================
# Build Configurations
# ============================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build types
set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo;Release")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Default policies
cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
cmake_policy(SET CMP0122 NEW)  # C++20 modules

# ============================================================
# Compiler Options
# ============================================================
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wzero-as-null-pointer-constant -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wnull-dereference -Wformat=2 -Wformat-security -fdiagnostics-color=always -fno-rtti -fno-exceptions -ffp-contract=off -fno-strict-aliasing -std=c++20")

# Build type specific flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer -DDEBUG=1")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -g -DNDEBUG=1")
# Do not use -march=native: it makes release binaries host-specific and breaks
# reproducible packaging. See docs/RELEASE.md once packaging exists.
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -DNDEBUG=1")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fsanitize=address,undefined")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto -s")

# ============================================================
# FetchContent Dependencies (Header-only)
# ============================================================
include(FetchContent)

# doctest — Testing
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(doctest)

# nlohmann/json — JSON serialization
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# toml++ — TOML config parsing
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# miniaudio — vendored single header at src/platform/audio/miniaudio.h.
# Version is recorded in that header; there is no CMake download step.
# VMA is not used yet; no renderer exists.

# ============================================================
# System Dependencies
# ============================================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3>=3.2.0)
pkg_check_modules(VULKAN REQUIRED vulkan>=1.3)
pkg_check_modules(LIBPNG REQUIRED libpng>=1.6)
pkg_check_modules(PIPEWIRE pipewire-0.3)
pkg_check_modules(PULSEAUDIO libpulse)
pkg_check_modules(ALSA alsa)
find_package(fmt REQUIRED)

# Threads
find_package(Threads REQUIRED)
find_program(PYTHON3_EXECUTABLE NAMES python3)

# ============================================================
# Shader compilation (GLSL -> SPIR-V)
# ============================================================
find_program(GLSLC_EXECUTABLE glslc REQUIRED)
set(SOUL_KNIGHT_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
set(SOUL_KNIGHT_SPRITE_VERT_SPV "${SOUL_KNIGHT_SHADER_OUTPUT_DIR}/sprite.vert.spv")
set(SOUL_KNIGHT_SPRITE_FRAG_SPV "${SOUL_KNIGHT_SHADER_OUTPUT_DIR}/sprite.frag.spv")

add_custom_command(
    OUTPUT "${SOUL_KNIGHT_SPRITE_VERT_SPV}" "${SOUL_KNIGHT_SPRITE_FRAG_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SOUL_KNIGHT_SHADER_OUTPUT_DIR}"
    COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=vert
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.vert"
            -o "${SOUL_KNIGHT_SPRITE_VERT_SPV}"
    COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=frag
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.frag"
            -o "${SOUL_KNIGHT_SPRITE_FRAG_SPV}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.frag"
    COMMENT "Compiling Sprite Shaders"
    VERBATIM
)
add_custom_target(soul-knight-shaders ALL
    DEPENDS "${SOUL_KNIGHT_SPRITE_VERT_SPV}" "${SOUL_KNIGHT_SPRITE_FRAG_SPV}"
)

# Runtime assets are resolved relative to the executable by FileSystem. Copy the
# synthesized test PNGs and compiled SPIR-V there so the runtime is independent
# of the caller's current working directory.
set(SOUL_KNIGHT_ASSET_OUTPUT_DIR "${CMAKE_BINARY_DIR}/assets")
add_custom_target(soul-knight-assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${SOUL_KNIGHT_ASSET_OUTPUT_DIR}/test"
            "${SOUL_KNIGHT_ASSET_OUTPUT_DIR}/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/test"
            "${SOUL_KNIGHT_ASSET_OUTPUT_DIR}/test"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/analysis/assets/scenes"
            "${SOUL_KNIGHT_ASSET_OUTPUT_DIR}/test/scenes"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SOUL_KNIGHT_SPRITE_VERT_SPV}"
            "${SOUL_KNIGHT_ASSET_OUTPUT_DIR}/shaders/sprite.vert.spv"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SOUL_KNIGHT_SPRITE_FRAG_SPV}"
            "${SOUL_KNIGHT_ASSET_OUTPUT_DIR}/shaders/sprite.frag.spv"
    DEPENDS soul-knight-shaders
    COMMENT "Copying synthetic test assets and compiled shaders"
)

# Commit hash for startup diagnostics. Never fail configuration if Git is absent.
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE SOUL_KNIGHT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT SOUL_KNIGHT_COMMIT_HASH)
    set(SOUL_KNIGHT_COMMIT_HASH "unknown")
endif()

# ============================================================
# Platform-Specific Definitions
# ============================================================
add_compile_definitions(
    LITEKNIGHT_VERSION="${LITEKNIGHT_VERSION}"
    SOUL_KNIGHT_VERSION="${LITEKNIGHT_VERSION}"
    SOUL_KNIGHT_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    SOUL_KNIGHT_COMMIT_HASH="${SOUL_KNIGHT_COMMIT_HASH}"
    SDL_MAIN_HANDLED
    NLOHMANN_JSON_NAMESPACE=nlohmann
)

# ============================================================
# Library: core (logging and foundational types)
# ============================================================
add_library(core STATIC
    src/engine/core/Log.cpp
    src/engine/core/Log.hpp
    src/engine/core/Types.hpp
)

target_include_directories(core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(core PUBLIC
    fmt::fmt
)

# ============================================================
# Library: platform (SDL3, miniaudio, filesystem, timing)
# ============================================================
add_library(platform STATIC
    src/platform/wayland/Window.cpp
    src/platform/wayland/Window.hpp
    src/platform/input/InputManager.cpp
    src/platform/input/InputManager.hpp
    src/platform/input/Action.hpp
    src/platform/timing/FrameTimer.cpp
    src/platform/timing/FrameTimer.hpp
    src/platform/filesystem/FileSystem.cpp
    src/platform/filesystem/FileSystem.hpp
    src/platform/audio/AudioEngine.cpp
    src/platform/audio/AudioEngine.hpp
    src/platform/audio/miniaudio.h
)

target_include_directories(platform PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    ${SDL3_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIRS}
)

target_link_libraries(platform PUBLIC
    core
    ${SDL3_LIBRARIES}
    ${VULKAN_LIBRARIES}
    Threads::Threads
)

# ============================================================
# Library: engine (core, math, ecs, rendering, resources, audio, collision)
# ============================================================
add_library(engine STATIC
    # Math
    src/engine/math/Vec2.cpp
    src/engine/math/Vec2.hpp
    src/engine/math/Vec3.hpp
    src/engine/math/Mat4.hpp
    src/engine/math/Random.cpp
    src/engine/math/Random.hpp
    src/engine/math/UnityRandom.cpp
    src/engine/math/UnityRandom.hpp
    src/engine/math/Rect.hpp
    src/engine/math/Color.hpp
    # ECS
    src/engine/ecs/Entity.hpp
    src/engine/ecs/ComponentPool.hpp
    src/engine/ecs/World.hpp
    src/engine/ecs/System.hpp
    # Rendering
    src/engine/rendering/Camera.hpp
    src/engine/rendering/SpriteBatch.cpp
    src/engine/rendering/SpriteBatch.hpp
    src/engine/rendering/LogicalViewport.cpp
    src/engine/rendering/LogicalViewport.hpp
    src/engine/rendering/BitmapFont.cpp
    src/engine/rendering/BitmapFont.hpp
    src/engine/rendering/TextRenderer.cpp
    src/engine/rendering/TextRenderer.hpp
    src/engine/rendering/DebugText.hpp
    src/engine/rendering/Texture2D.cpp
    src/engine/rendering/Texture2D.hpp
    src/engine/rendering/VulkanContext.cpp
    src/engine/rendering/VulkanContext.hpp
    src/engine/rendering/SpriteRenderer.cpp
    src/engine/rendering/SpriteRenderer.hpp
    src/engine/rendering/VulkanRenderer.cpp
    src/engine/rendering/VulkanRenderer.hpp
    # Locomotion / collision
    src/engine/collision/AabbCollision.cpp
    src/engine/collision/AabbCollision.hpp
    src/engine/locomotion/PlayerLocomotion.cpp
    src/engine/locomotion/PlayerLocomotion.hpp
    src/engine/locomotion/FacingDirection.hpp
    # Rendering: explicit player scene binding
    src/engine/rendering/PlayerRenderSync.cpp
    src/engine/rendering/PlayerRenderSync.hpp
    # Animation
    src/engine/animation/AnimationClip.cpp
    src/engine/animation/AnimationClip.hpp
    src/engine/animation/AnimationClipLoader.cpp
    src/engine/animation/AnimationClipLoader.hpp
    src/engine/animation/AnimationPlayer.cpp
    src/engine/animation/AnimationPlayer.hpp
    src/engine/animation/LocomotionAnimator.cpp
    src/engine/animation/LocomotionAnimator.hpp
    # Asset decoding / native scene
    src/engine/assets/TextureData.hpp
    src/engine/assets/TextureSampling.hpp
    src/engine/assets/PngDecoder.cpp
    src/engine/assets/PngDecoder.hpp
    src/engine/assets/SpriteData.hpp
    src/engine/assets/SceneAsset.hpp
    src/engine/assets/DoorAnimation.cpp
    src/engine/assets/DoorAnimation.hpp
    src/engine/assets/SceneLoader.cpp
    src/engine/assets/SceneLoader.hpp
    src/engine/rendering/Camera2D.cpp
    src/engine/rendering/Camera2D.hpp
    # src/engine/rendering/Shader.hpp
    # src/engine/rendering/DebugRenderer.hpp
    # Resources
    # src/engine/resources/AssetManager.cpp
    # src/engine/resources/AssetManager.hpp
    # src/engine/resources/TextureLoader.cpp
    # src/engine/resources/TextureLoader.hpp
    # src/engine/resources/AudioLoader.cpp
    # src/engine/resources/AudioLoader.hpp
    # src/engine/resources/DataLoader.cpp
    # src/engine/resources/DataLoader.hpp
    # Audio
    # src/engine/audio/AudioManager.cpp
    # src/engine/audio/AudioManager.hpp
    # Collision
    # src/engine/collision/Collision.cpp
    # src/engine/collision/Collision.hpp
    # src/engine/collision/Broadphase.cpp
    # src/engine/collision/Broadphase.hpp
)

target_include_directories(engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(engine PUBLIC
    platform
    ${VULKAN_LIBRARIES}
    ${LIBPNG_LIBRARIES}
    nlohmann_json::nlohmann_json
)

target_include_directories(engine PUBLIC
    ${LIBPNG_INCLUDE_DIRS}
)

add_dependencies(engine soul-knight-shaders)

# ============================================================
# Library: game (Soul Knight specific systems)
# ============================================================
add_library(game STATIC
    # Player
    src/game/player/PlayerController.cpp
    src/game/player/PlayerController.hpp
    src/game/player/PlayerInput.cpp
    src/game/player/PlayerInput.hpp
    src/game/player/Character.cpp
    src/game/player/Character.hpp
    src/game/player/PlayerCharacterSet.cpp
    src/game/player/PlayerCharacterSet.hpp
    # Rooms
    src/game/room/Door.cpp
    src/game/room/Door.hpp
    src/game/room/ObstacleField.cpp
    src/game/room/ObstacleField.hpp
    src/game/room/RoomDoorController.cpp
    src/game/room/RoomDoorController.hpp
    src/game/room/RoomManager.cpp
    src/game/room/RoomManager.hpp
    src/game/room/DungeonPlan.cpp
    src/game/room/DungeonPlan.hpp
    # Combat
    src/game/combat/Damage.hpp
    src/game/combat/ProjectileSystem.cpp
    src/game/combat/ProjectileSystem.hpp
    src/game/combat/Weapon.cpp
    src/game/combat/Weapon.hpp
    src/game/weapons/WeaponRegistry.cpp
    src/game/weapons/WeaponRegistry.hpp
    src/game/weapons/WeaponRenderer.cpp
    src/game/weapons/WeaponRenderer.hpp
    src/game/weapons/WeaponInventory.cpp
    src/game/weapons/WeaponInventory.hpp
    src/game/weapons/WeaponPickup.cpp
    src/game/weapons/WeaponPickup.hpp
    src/game/save/SaveData.hpp
    src/game/save/SaveSerializer.cpp
    src/game/save/SaveSerializer.hpp
    src/game/save/SaveManager.cpp
    src/game/save/SaveManager.hpp
    src/game/hub/PlantSystem.cpp
    src/game/hub/PlantSystem.hpp
    src/game/items/ItemAssets.cpp
    src/game/items/ItemAssets.hpp
    src/game/items/ItemWorld.cpp
    src/game/items/ItemWorld.hpp
    src/game/ui/DevOverlay.cpp
    src/game/ui/DevOverlay.hpp
    src/game/ui/AssetLab.cpp
    src/game/ui/AssetLab.hpp
    src/game/ui/UIRenderer.cpp
    src/game/ui/UIRenderer.hpp
    src/game/ui/GameUI.cpp
    src/game/ui/GameUI.hpp
    src/game/ui/HudLayout.cpp
    src/game/ui/HudLayout.hpp
    src/game/ui/UIAssets.cpp
    src/game/ui/UIAssets.hpp
    src/game/test/InteractiveTest.cpp
    src/game/test/InteractiveTest.hpp
    src/game/test/InteractiveReplay.cpp
    src/game/test/InteractiveReplay.hpp
    src/game/enemy/EnemySystem.cpp
    src/game/enemy/EnemySystem.hpp
    # Characters
    # src/game/characters/CharacterData.cpp
    # src/game/characters/CharacterData.hpp
    # Enemies
    # src/game/enemies/EnemyController.cpp
    # src/game/enemies/EnemyController.hpp
    # src/game/enemies/EnemyAI.cpp
    # src/game/enemies/EnemyAI.hpp
    # src/game/enemies/EnemyData.cpp
    # src/game/enemies/EnemyData.hpp
    # src/game/enemies/BossController.cpp
    # src/game/enemies/BossController.hpp
    # Weapons
    # src/game/weapons/WeaponBase.cpp
    # src/game/weapons/WeaponBase.hpp
    # src/game/weapons/WeaponData.cpp
    # src/game/weapons/WeaponData.hpp
    # src/game/weapons/MeleeWeapon.cpp
    # src/game/weapons/MeleeWeapon.hpp
    # src/game/weapons/RangedWeapon.cpp
    # src/game/weapons/RangedWeapon.hpp
    # src/game/weapons/SpecialWeapon.cpp
    # src/game/weapons/SpecialWeapon.hpp
    # Projectiles
    # src/game/projectiles/ProjectileBase.cpp
    # src/game/projectiles/ProjectileBase.hpp
    # src/game/projectiles/ProjectileData.cpp
    # src/game/projectiles/ProjectileData.hpp
    # src/game/projectiles/TriggerSystem.cpp
    # src/game/projectiles/TriggerSystem.hpp
    # Items
    # src/game/items/ItemBase.cpp
    # src/game/items/ItemBase.hpp
    # src/game/items/ItemData.cpp
    # src/game/items/ItemData.hpp
    # src/game/items/Chest.cpp
    # src/game/items/Chest.hpp
    # src/game/items/Buff.cpp
    # src/game/items/Buff.hpp
    # Rooms
    # src/game/rooms/RoomTemplate.cpp
    # src/game/rooms/RoomTemplate.hpp
    # src/game/rooms/RoomManager.cpp
    # src/game/rooms/RoomManager.hpp
    # src/game/rooms/Door.cpp
    # src/game/rooms/Door.hpp
    # Generation
    # src/game/generation/MapManager.cpp
    # src/game/generation/MapManager.hpp
    # src/game/generation/DungeonGenerator.cpp
    # src/game/generation/DungeonGenerator.hpp
    # src/game/generation/SerializeLinker.cpp
    # src/game/generation/SerializeLinker.hpp
    # src/game/generation/RandomTile.cpp
    # src/game/generation/RandomTile.hpp
    # Combat
    # src/game/combat/CombatSystem.cpp
    # src/game/combat/CombatSystem.hpp
    # src/game/combat/DamageSystem.cpp
    # src/game/combat/DamageSystem.hpp
    # src/game/combat/StatusEffect.cpp
    # src/game/combat/StatusEffect.hpp
    # Status
    # src/game/status/StatusEffectSystem.cpp
    # src/game/status/StatusEffectSystem.hpp
    # Progression
    # src/game/progression/ProgressionSystem.cpp
    # src/game/progression/ProgressionSystem.hpp
    # src/game/progression/UnlockManager.cpp
    # src/game/progression/UnlockManager.hpp
    # NPCs
    # src/game/npc/NPCController.cpp
    # src/game/npc/NPCController.hpp
    # Bosses
    # src/game/bosses/BossData.cpp
    # src/game/bosses/BossData.hpp
    # UI
    # src/game/ui/HUD.cpp
    # src/game/ui/HUD.hpp
    # src/game/ui/MenuSystem.cpp
    # src/game/ui/MenuSystem.hpp
    # src/game/ui/InventoryUI.cpp
    # src/game/ui/InventoryUI.hpp
    # Save
    # src/game/save/SaveManager.cpp
    # src/game/save/SaveManager.hpp
    # src/game/save/GameStateSerializer.cpp
    # src/game/save/GameStateSerializer.hpp
    # src/game/save/SaveMigrations.cpp
    # src/game/save/SaveMigrations.hpp
)

target_include_directories(game PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(game PUBLIC
    engine
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json
)

# ============================================================
# Library: app (application lifecycle)
# ============================================================
add_library(app STATIC
    src/app/AssetEnvironment.cpp
    src/app/AssetEnvironment.hpp
    src/app/AppConfig.cpp
    src/app/AppConfig.hpp
    src/app/Application.cpp
    src/app/Application.hpp
)

target_include_directories(app PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(app PUBLIC
    game
    engine
    platform
)

add_dependencies(app soul-knight-assets)

# ============================================================
# Executable: soul-knight (Main Game)
# ============================================================
add_executable(soul-knight
    src/app/main.cpp
)

target_link_libraries(soul-knight PRIVATE
    app
)

add_dependencies(soul-knight soul-knight-assets)

# First-class developer launcher. This is a tiny script because it uses the
# same tested executable and therefore the same binary-relative asset lookup.
set(LITEKNIGHT_DEVROOM_LAUNCHER "${CMAKE_BINARY_DIR}/bin/liteknight-devroom")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/liteknight-devroom.in"
    "${LITEKNIGHT_DEVROOM_LAUNCHER}"
    @ONLY
)
file(CHMOD "${LITEKNIGHT_DEVROOM_LAUNCHER}"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                 GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
add_custom_target(liteknight-devroom ALL
    DEPENDS soul-knight
    COMMENT "Preparing build/bin/liteknight-devroom"
)

# ============================================================
# Tool: native PNG round-trip verifier
# ============================================================
add_executable(soul-knight-png-roundtrip
    src/tools/png_roundtrip.cpp
)

target_link_libraries(soul-knight-png-roundtrip PRIVATE
    engine
)

# ============================================================
# Executable: soul-knight-test (Unit Tests)
# ============================================================
add_executable(soul-knight-test
    tests/unit/main.cpp
    # Unit tests
    tests/unit/math_test.cpp
    tests/unit/ecs_test.cpp
    tests/unit/rng_test.cpp
    tests/unit/unity_random_test.cpp
    tests/unit/input_test.cpp
    tests/unit/timing_test.cpp
    tests/unit/platform_test.cpp
    tests/unit/render_test.cpp
    tests/unit/png_test.cpp
    tests/unit/scene_test.cpp
    tests/unit/room_visual_test.cpp
    tests/unit/door_test.cpp
    tests/unit/player_render_sync_test.cpp
    tests/unit/direction_test.cpp
    tests/unit/character_switch_test.cpp
    tests/unit/obstacle_field_test.cpp
    tests/unit/combat_test.cpp
    tests/unit/devroom_test.cpp
    tests/unit/animation_test.cpp
    tests/unit/locomotion_test.cpp
    tests/unit/energy_inventory_test.cpp
    tests/unit/enemy_ai_test.cpp
    tests/unit/weapon_pickup_test.cpp
    tests/unit/save_test.cpp
    tests/unit/replay_test.cpp
    tests/unit/foundation_test.cpp
    tests/unit/plant_test.cpp
    tests/unit/room_manager_test.cpp
    tests/unit/ability_test.cpp
    tests/unit/ui_assets_test.cpp
    tests/unit/ui_test.cpp
    tests/unit/ui_layout_test.cpp
    tests/unit/world_depth_test.cpp
    tests/unit/item_test.cpp
    # Integration tests
    # tests/integration/asset_loading_test.cpp
    # tests/integration/save_load_test.cpp
    # Gameplay tests
    # tests/gameplay/player_movement_test.cpp
    # tests/gameplay/combat_test.cpp
    # tests/gameplay/generation_test.cpp
    # Regression tests
    # tests/regression/bug_fixes_test.cpp
    # Serialization tests
    # tests/serialization/save_roundtrip_test.cpp
    # Procedural tests
    # tests/procedural/determinism_test.cpp
    # Performance tests
    # tests/performance/benchmark_test.cpp
)

target_link_libraries(soul-knight-test PRIVATE
    app
    game
    engine
    platform
    doctest::doctest
)

add_dependencies(soul-knight-test soul-knight-assets)

# Doctest configuration. Do NOT define DOCTEST_CONFIG_DISABLE: doctest checks
# only whether it is defined, so even DOCTEST_CONFIG_DISABLE=0 compiles all
# tests out.
target_compile_definitions(soul-knight-test PRIVATE
    DOCTEST_CONFIG_NO_POSIX_SIGNALS=1
    "SOUL_KNIGHT_TEST_ASSET_DIR=\"${SOUL_KNIGHT_ASSET_OUTPUT_DIR}/test\""
)

# ============================================================
# Install (Optional)
# ============================================================
install(TARGETS soul-knight
    RUNTIME DESTINATION bin
)

install(DIRECTORY assets/
    DESTINATION share/soul-knight/assets
    FILES_MATCHING PATTERN "*.ktx2" PATTERN "*.ogg" PATTERN "*.json" PATTERN "*.toml"
)

# The Phase 4 smoke executable requires its synthetic PNG fixture and compiled
# shaders at the same executable-relative/system asset root used by FileSystem.
install(DIRECTORY assets/test
    DESTINATION share/soul-knight/assets
    FILES_MATCHING PATTERN "*.png"
)
install(FILES
    "${SOUL_KNIGHT_SPRITE_VERT_SPV}"
    "${SOUL_KNIGHT_SPRITE_FRAG_SPV}"
    DESTINATION share/soul-knight/assets/shaders
)

install(FILES config/default.toml
    DESTINATION share/soul-knight/config
)

# ============================================================
# CTest Integration
# ============================================================
enable_testing()

# Only run categories that actually contain tests. Do not add empty CTest
# entries: a test entry that selects zero doctest cases reports green.
add_test(NAME soul-knight-unit-tests
    COMMAND soul-knight-test --test-suite=unit --reporter=console
)
if(PYTHON3_EXECUTABLE)
    add_test(NAME liteknight-asset-audit
        COMMAND "${PYTHON3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/asset/audit_assets.py"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(NAME liteknight-hero-room-audit
        COMMAND "${PYTHON3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/asset/audit_assets.py"
                --scene "${CMAKE_CURRENT_SOURCE_DIR}/analysis/assets/scenes/hero_room.json"
                --inventory "${CMAKE_CURRENT_SOURCE_DIR}/analysis/assets/asset_inventory.json"
                --generated-root "${CMAKE_CURRENT_SOURCE_DIR}/assets/generated/visual/hero_room"
                --report-dir "${CMAKE_CURRENT_BINARY_DIR}/hero-room-audit"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()
# FAIL_REGULAR_EXPRESSION (not PASS) so a real test failure still turns CTest
# red. This only rejects the old false-green case where the filter selected zero
# test cases.
set_tests_properties(soul-knight-unit-tests PROPERTIES
    FAIL_REGULAR_EXPRESSION "test cases:[ ]+0[ ]"
)

# ============================================================
# Custom Targets
# ============================================================
add_custom_target(format
    COMMAND ${CMAKE_COMMAND} -E echo "Run ./tools/format.sh fix to format code"
    COMMENT "Code formatting (run ./tools/format.sh fix)"
)

add_custom_target(check-format
    COMMAND ${CMAKE_COMMAND} -E echo "Run ./tools/format.sh check to verify formatting"
    COMMENT "Check code formatting (run ./tools/format.sh check)"
)

# ============================================================
# CPack (Packaging)
# ============================================================
set(CPACK_PACKAGE_NAME "soul-knight")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native Linux port of Soul Knight 2.6.8")
set(CPACK_PACKAGE_VENDOR "Soul Knight Linux Project")
# CachyOS is Arch-based. A DEB package is not the target; the real Phase 10
# deliverable is a PKGBUILD. Keep a generic TGZ archive until that exists.
set(CPACK_GENERATOR "TGZ")
include(CPack)