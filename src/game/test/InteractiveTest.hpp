// Soul Knight Linux — serialized interactive devroom test recorder
#pragma once

#include "engine/math/Vec2.hpp"
#include "platform/input/InputSnapshot.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace soulknight::game::test {

struct TestState {
    engine::Vec2 position{0.0f, 0.0f};
    engine::Vec2 velocity{0.0f, 0.0f};
    float camera_x = 0.0f;
    float camera_y = 0.0f;
    std::string animation_state;
    std::string animation_frame;
    std::string character;
    std::string weapon;
    std::string room;
    int hp = 0;
    int max_hp = 0;
    int armor = 0;
    int energy = 0;
    int max_energy = 0;
    std::size_t enemies = 0;
    std::size_t projectiles = 0;
    bool dead = false;
    bool doors_open = false;
};

class InteractiveTest {
public:
    ~InteractiveTest();
    bool Start(const std::string& name,
               const std::string& instructions,
               const std::string& version,
               const std::string& commit,
               const std::string& scene_id,
               const std::string& scene_sha256,
               const std::filesystem::path& output_directory);
    bool Active() const { return active_; }
    const std::string& Name() const { return name_; }
    const std::string& Instructions() const { return instructions_; }
    void RecordTick(std::uint64_t tick,
                    const platform::InputSnapshot& input,
                    const TestState& state);
    // Raw key commands (F5/F6/F9/F11/F12/O/C/P/R and menu navigation) are not
    // part of InputSnapshot. They are serialized separately so replay can
    // reproduce debug-menu and spawn actions as well as movement/attacks.
    void RecordCommand(std::uint64_t tick, int scancode, int modifiers);
    void SetAssetHashes(const std::string& reference_apk_sha256,
                        const std::string& asset_inventory_hash,
                        const std::string& scene_sha256);
    bool Finalize(const std::string& result, const std::string& notes);
    bool Abort(const std::string& reason);
    std::string OutputPath() const { return output_path_; }

private:
    bool Write(const std::string& result, const std::string& notes);
    void AppendInput(std::uint64_t tick, const platform::InputSnapshot& input);
    void AppendState(std::uint64_t tick, const TestState& state);

    bool active_ = false;
    std::string name_;
    std::string instructions_;
    std::string version_;
    std::string commit_;
    std::string scene_id_;
    std::string scene_sha256_;
    std::filesystem::path output_directory_;
    std::string started_at_;
    std::string output_path_;
    std::uint64_t last_state_tick_ = 0;
    bool have_state_ = false;
    TestState last_state_;
    // Opaque JSON document storage is kept in the implementation so the header
    // does not expose a serialization dependency.
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace soulknight::game::test
