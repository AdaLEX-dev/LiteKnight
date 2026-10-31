#include "InteractiveTest.hpp"

#include "platform/input/Action.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace soulknight::game::test {
namespace {

std::string Timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

std::string FileTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::ostringstream stream;
    stream << std::put_time(&local, "%Y-%m-%d_%H-%M-%S");
    return stream.str();
}

std::string ActionName(platform::Action action) {
    switch (action) {
        case platform::Action::MoveUp: return "MoveUp";
        case platform::Action::MoveDown: return "MoveDown";
        case platform::Action::MoveLeft: return "MoveLeft";
        case platform::Action::MoveRight: return "MoveRight";
        case platform::Action::Attack: return "Attack";
        case platform::Action::AimUp: return "AimUp";
        case platform::Action::AimDown: return "AimDown";
        case platform::Action::AimLeft: return "AimLeft";
        case platform::Action::AimRight: return "AimRight";
        case platform::Action::Skill: return "Skill";
        case platform::Action::Dodge: return "Dodge";
        case platform::Action::Interact: return "Interact";
        case platform::Action::WeaponSwitch: return "WeaponSwitch";
        case platform::Action::WeaponSlot1: return "WeaponSlot1";
        case platform::Action::WeaponSlot2: return "WeaponSlot2";
        case platform::Action::WeaponSlot3: return "WeaponSlot3";
        case platform::Action::DebugToggle: return "DebugToggle";
        case platform::Action::DebugNoclip: return "DebugNoclip";
        case platform::Action::DebugKillAll: return "DebugKillAll";
        case platform::Action::DebugSpawnEnemy: return "DebugSpawnEnemy";
        case platform::Action::COUNT: break;
        default: break;
    }
    return "Other";
}

nlohmann::json ActionEdges(const std::bitset<platform::ACTION_COUNT>& bits) {
    nlohmann::json result = nlohmann::json::array();
    for (std::size_t i = 0; i < platform::ACTION_COUNT; ++i) {
        if (bits.test(i)) {
            result.push_back(ActionName(static_cast<platform::Action>(i)));
        }
    }
    return result;
}

} // namespace

struct InteractiveTest::Impl {
    nlohmann::json document = nlohmann::json::object();
    nlohmann::json inputs = nlohmann::json::array();
    nlohmann::json commands = nlohmann::json::array();
    nlohmann::json states = nlohmann::json::array();
};

InteractiveTest::~InteractiveTest() {
    delete impl_;
    impl_ = nullptr;
}

bool InteractiveTest::Start(const std::string& name,
                            const std::string& instructions,
                            const std::string& version,
                            const std::string& commit,
                            const std::string& scene_id,
                            const std::string& scene_sha256,
                            const std::filesystem::path& output_directory) {
    if (active_) return false;
    name_ = name;
    instructions_ = instructions;
    version_ = version;
    commit_ = commit;
    scene_id_ = scene_id;
    scene_sha256_ = scene_sha256;
    output_directory_ = output_directory;
    started_at_ = Timestamp();
    output_path_ = (output_directory_ /
        (FileTimestamp() + "_" + name_ + ".json")).string();
    last_state_tick_ = 0;
    have_state_ = false;
    if (impl_ == nullptr) impl_ = new Impl();
    impl_->document.clear();
    impl_->inputs = nlohmann::json::array();
    impl_->commands = nlohmann::json::array();
    impl_->states = nlohmann::json::array();
    impl_->document["test_name"] = name_;
    impl_->document["version"] = version_;
    impl_->document["commit"] = commit_;
    impl_->document["timestamp"] = started_at_;
    impl_->document["platform"] = "Wayland";
    impl_->document["renderer"] = "Vulkan";
    impl_->document["scene_id"] = scene_id_;
    impl_->document["scene_sha256"] = scene_sha256_;
    impl_->document["instructions"] = instructions_;
    impl_->document["result"] = "RUNNING";
    impl_->document["notes"] = "";
    active_ = true;
    return true;
}

void InteractiveTest::AppendInput(std::uint64_t tick,
                                  const platform::InputSnapshot& input) {
    nlohmann::json entry;
    entry["tick"] = tick;
    entry["move_x"] = input.move_x;
    entry["move_y"] = input.move_y;
    entry["aim_x"] = input.aim_x;
    entry["aim_y"] = input.aim_y;
    entry["pressed"] = ActionEdges(input.pressed);
    entry["released"] = ActionEdges(input.released);
    entry["held"] = ActionEdges(input.held);
    impl_->inputs.push_back(std::move(entry));
}

void InteractiveTest::AppendState(std::uint64_t tick, const TestState& state) {
    nlohmann::json entry;
    entry["tick"] = tick;
    entry["player"] = {state.position.x, state.position.y};
    entry["velocity"] = {state.velocity.x, state.velocity.y};
    entry["camera"] = {state.camera_x, state.camera_y};
    entry["animation_state"] = state.animation_state;
    entry["animation_frame"] = state.animation_frame;
    entry["character"] = state.character;
    entry["weapon"] = state.weapon;
    entry["room"] = state.room;
    entry["hp"] = state.hp;
    entry["max_hp"] = state.max_hp;
    entry["armor"] = state.armor;
    entry["energy"] = state.energy;
    entry["max_energy"] = state.max_energy;
    entry["enemies"] = state.enemies;
    entry["projectiles"] = state.projectiles;
    entry["dead"] = state.dead;
    entry["doors_open"] = state.doors_open;
    impl_->states.push_back(std::move(entry));
}

void InteractiveTest::RecordCommand(std::uint64_t tick, int scancode,
                                    int modifiers) {
    if (!active_ || impl_ == nullptr) return;
    nlohmann::json entry;
    entry["tick"] = tick;
    entry["scancode"] = scancode;
    entry["modifiers"] = modifiers;
    impl_->commands.push_back(std::move(entry));
}

void InteractiveTest::RecordTick(std::uint64_t tick,
                                 const platform::InputSnapshot& input,
                                 const TestState& state) {
    if (!active_ || impl_ == nullptr) return;
    AppendInput(tick, input);
    // Keep a full deterministic input stream but store state snapshots at 5 Hz
    // plus whenever a visible state field changes. Input remains the replay
    // source; state snapshots are for diagnosis.
    bool changed = !have_state_ ||
                   state.position.x != last_state_.position.x ||
                   state.position.y != last_state_.position.y ||
                   state.hp != last_state_.hp ||
                   state.armor != last_state_.armor ||
                   state.energy != last_state_.energy ||
                   state.dead != last_state_.dead ||
                   state.character != last_state_.character ||
                   state.weapon != last_state_.weapon ||
                   state.animation_state != last_state_.animation_state ||
                   state.animation_frame != last_state_.animation_frame ||
                   state.enemies != last_state_.enemies ||
                   state.projectiles != last_state_.projectiles;
    if (changed || tick % 10 == 0 || !have_state_) {
        AppendState(tick, state);
        last_state_ = state;
        have_state_ = true;
        last_state_tick_ = tick;
    }
}

void InteractiveTest::SetAssetHashes(const std::string& reference_apk_sha256,
                                     const std::string& asset_inventory_hash,
                                     const std::string& scene_sha256) {
    if (!active_ || impl_ == nullptr) return;
    impl_->document["asset_hashes"] = {
        {"reference_apk_sha256", reference_apk_sha256},
        {"asset_inventory_hash", asset_inventory_hash},
        {"asset_inventory_hash_algorithm", "fnv1a64"},
        {"scene_sha256", scene_sha256}
    };
}

bool InteractiveTest::Write(const std::string& result,
                            const std::string& notes) {
    if (impl_ == nullptr) return false;
    impl_->document["result"] = result;
    impl_->document["notes"] = notes;
    impl_->document["finished_at"] = Timestamp();
    impl_->document["inputs"] = impl_->inputs;
    impl_->document["commands"] = impl_->commands;
    impl_->document["state_samples"] = impl_->states;
    std::error_code error;
    std::filesystem::create_directories(output_directory_, error);
    const std::filesystem::path temporary = output_path_ + ".tmp";
    {
        std::ofstream stream(temporary);
        if (!stream) return false;
        stream << std::setw(2) << impl_->document << '\n';
        if (!stream.good()) return false;
    }
    std::filesystem::rename(temporary, std::filesystem::path(output_path_), error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

bool InteractiveTest::Finalize(const std::string& result,
                               const std::string& notes) {
    if (!active_) return false;
    const bool written = Write(result, notes);
    active_ = false;
    return written;
}

bool InteractiveTest::Abort(const std::string& reason) {
    return Finalize("ABORTED", reason);
}

} // namespace soulknight::game::test
