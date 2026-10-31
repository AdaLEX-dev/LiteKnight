// Soul Knight Linux — deterministic replay of serialized interactive inputs
#pragma once

#include "platform/input/InputSnapshot.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace soulknight::game::test {

// The recorder serializes one InputSnapshot per fixed tick. Replay feeds those
// snapshots back into the same fixed simulation. Menu/asset-lab selections and
// filesystem save/load commands are not part of InputSnapshot yet; this first
// replay slice is limited to movement, aim, attack, weapon-slot actions and
// interact. The limitation is explicit in the replay report.
class InteractiveReplay {
public:
    struct Command {
        std::uint64_t tick = 0;
        int scancode = 0;
        int modifiers = 0;
    };

    bool Load(const std::filesystem::path& path, std::string* error);
    bool Active() const { return active_; }
    bool Finished() const { return cursor_ >= inputs_.size(); }
    std::size_t InputCount() const { return inputs_.size(); }
    std::uint64_t FirstTick() const { return first_tick_; }
    const std::string& TestName() const { return test_name_; }
    const std::string& Commit() const { return commit_; }
    const std::filesystem::path& Path() const { return path_; }

    // Returns false once the serialized stream is exhausted.
    bool Next(platform::InputSnapshot* snapshot);
    const std::vector<Command>& CommandsForTick(std::uint64_t tick) const;
    const platform::InputSnapshot& LastSnapshot() const { return last_snapshot_; }

private:
    bool active_ = false;
    std::filesystem::path path_;
    std::string test_name_;
    std::string commit_;
    std::uint64_t first_tick_ = 0;
    std::size_t cursor_ = 0;
    std::vector<platform::InputSnapshot> inputs_;
    std::vector<Command> commands_;
    platform::InputSnapshot last_snapshot_{};
};

} // namespace soulknight::game::test
