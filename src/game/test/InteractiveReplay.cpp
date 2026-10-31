#include "InteractiveReplay.hpp"

#include "platform/input/Action.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

#include <fstream>

namespace soulknight::game::test {
namespace {

using Json = nlohmann::json;

bool ActionFromName(const std::string& name, platform::Action* action) {
    struct Entry { const char* name; platform::Action action; };
    static constexpr Entry kEntries[] = {
        {"MoveUp", platform::Action::MoveUp},
        {"MoveDown", platform::Action::MoveDown},
        {"MoveLeft", platform::Action::MoveLeft},
        {"MoveRight", platform::Action::MoveRight},
        {"Attack", platform::Action::Attack},
        {"AimUp", platform::Action::AimUp},
        {"AimDown", platform::Action::AimDown},
        {"AimLeft", platform::Action::AimLeft},
        {"AimRight", platform::Action::AimRight},
        {"Skill", platform::Action::Skill},
        {"Dodge", platform::Action::Dodge},
        {"Interact", platform::Action::Interact},
        {"WeaponSwitch", platform::Action::WeaponSwitch},
        {"WeaponSlot1", platform::Action::WeaponSlot1},
        {"WeaponSlot2", platform::Action::WeaponSlot2},
        {"WeaponSlot3", platform::Action::WeaponSlot3},
        {"DebugToggle", platform::Action::DebugToggle},
        {"DebugNoclip", platform::Action::DebugNoclip},
        {"DebugKillAll", platform::Action::DebugKillAll},
        {"DebugSpawnEnemy", platform::Action::DebugSpawnEnemy},
    };
    for (const Entry& entry : kEntries) {
        if (name == entry.name) {
            *action = entry.action;
            return true;
        }
    }
    return false;
}

bool ReadActionSet(const Json& value, std::bitset<platform::ACTION_COUNT>* bits,
                   std::string* error) {
    if (!value.is_array()) {
        if (error != nullptr) *error = "replay action set is not an array";
        return false;
    }
    for (const Json& item : value) {
        if (!item.is_string()) {
            if (error != nullptr) *error = "replay action name is not a string";
            return false;
        }
        platform::Action action = platform::Action::COUNT;
        if (!ActionFromName(item.get<std::string>(), &action)) {
            // Unknown future actions are skipped rather than corrupting replay.
            continue;
        }
        bits->set(static_cast<std::size_t>(action));
    }
    return true;
}

float ReadNumber(const Json& object, const char* key) {
    return object.contains(key) && object[key].is_number()
        ? object[key].get<float>() : 0.0f;
}

} // namespace

bool InteractiveReplay::Load(const std::filesystem::path& path,
                             std::string* error) {
    if (error != nullptr) error->clear();
    std::ifstream stream(path);
    if (!stream) {
        if (error != nullptr) *error = "cannot open replay: " + path.string();
        return false;
    }
    Json document = Json::parse(stream, nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        if (error != nullptr) *error = "replay JSON is corrupt";
        return false;
    }
    if (!document.contains("inputs") || !document["inputs"].is_array()) {
        if (error != nullptr) *error = "replay has no inputs array";
        return false;
    }
    test_name_ = document.value("test_name", std::string{"unknown"});
    commit_ = document.value("commit", std::string{});
    inputs_.clear();
    commands_.clear();
    if (document.contains("commands") && document["commands"].is_array()) {
        for (const Json& item : document["commands"]) {
            if (!item.is_object() || !item.contains("tick") ||
                !item["tick"].is_number_integer() ||
                !item.contains("scancode") ||
                !item["scancode"].is_number_integer()) {
                if (error != nullptr) *error = "replay command entry is invalid";
                return false;
            }
            Command command;
            command.tick = item["tick"].get<std::uint64_t>();
            command.scancode = item["scancode"].get<int>();
            if (item.contains("modifiers") && item["modifiers"].is_number_integer()) {
                command.modifiers = item["modifiers"].get<int>();
            }
            commands_.push_back(command);
        }
    }
    for (const Json& entry : document["inputs"]) {
        if (!entry.is_object() || !entry.contains("tick") ||
            !entry["tick"].is_number_integer()) {
            if (error != nullptr) *error = "replay input entry is invalid";
            return false;
        }
        platform::InputSnapshot snapshot;
        snapshot.move_x = ReadNumber(entry, "move_x");
        snapshot.move_y = ReadNumber(entry, "move_y");
        snapshot.aim_x = ReadNumber(entry, "aim_x");
        snapshot.aim_y = ReadNumber(entry, "aim_y");
        if ((entry.contains("pressed") &&
             !ReadActionSet(entry["pressed"], &snapshot.pressed, error)) ||
            (entry.contains("released") &&
             !ReadActionSet(entry["released"], &snapshot.released, error)) ||
            (entry.contains("held") &&
             !ReadActionSet(entry["held"], &snapshot.held, error))) {
            return false;
        }
        inputs_.push_back(std::move(snapshot));
    }
    if (inputs_.empty()) {
        if (error != nullptr) *error = "replay input stream is empty";
        return false;
    }
    first_tick_ = document["inputs"][0]["tick"].get<std::uint64_t>();
    path_ = path;
    cursor_ = 0;
    active_ = true;
    return true;
}

const std::vector<InteractiveReplay::Command>&
InteractiveReplay::CommandsForTick(std::uint64_t tick) const {
    static const std::vector<Command> kEmpty;
    // Commands are serialized in chronological order. Returning a shared empty
    // vector keeps the caller allocation-free for ticks without commands.
    static thread_local std::vector<Command> filtered;
    filtered.clear();
    for (const Command& command : commands_) {
        if (command.tick == tick) filtered.push_back(command);
    }
    return filtered.empty() ? kEmpty : filtered;
}

bool InteractiveReplay::Next(platform::InputSnapshot* snapshot) {
    if (!active_ || snapshot == nullptr || cursor_ >= inputs_.size()) {
        return false;
    }
    last_snapshot_ = inputs_[cursor_++];
    *snapshot = last_snapshot_;
    return true;
}

} // namespace soulknight::game::test
