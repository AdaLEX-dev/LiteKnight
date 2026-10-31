// Soul Knight Linux — serialized interactive replay loader tests
#include <doctest/doctest.h>

#include "game/test/InteractiveReplay.hpp"
#include "platform/input/Action.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

using soulknight::game::test::InteractiveReplay;
using soulknight::platform::Action;

namespace {

class TempReplayFile {
public:
    explicit TempReplayFile(const std::string& contents) {
        const auto stamp = std::chrono::steady_clock::now()
                               .time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("liteknight-replay-test-" + std::to_string(stamp) + ".json");
        std::ofstream stream(path_, std::ios::binary | std::ios::trunc);
        stream << contents;
    }
    ~TempReplayFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
    const std::filesystem::path& Path() const { return path_; }
private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("Replay reloads serialized input snapshots deterministically" *
          doctest::test_suite("unit")) {
    TempReplayFile file(R"({
        "test_name":"combat_live",
        "commit":"abc123",
        "inputs":[
            {"tick":1,"move_x":0.5,"move_y":0.0,"aim_x":1.0,"aim_y":0.0,
             "pressed":["Attack"],"released":[],"held":["Attack"]},
            {"tick":2,"move_x":0.0,"move_y":0.0,"aim_x":0.0,"aim_y":1.0,
             "pressed":[],"released":["Attack"],"held":[]}
        ],
        "commands":[
            {"tick":1,"scancode":68,"modifiers":0},
            {"tick":2,"scancode":40,"modifiers":0}
        ]
    })");
    InteractiveReplay replay;
    std::string error;
    CHECK(replay.Load(file.Path(), &error));
    CHECK(replay.Active());
    CHECK(replay.TestName() == "combat_live");
    CHECK(replay.Commit() == "abc123");
    CHECK(replay.InputCount() == 2);
    CHECK(replay.FirstTick() == 1);
    CHECK_FALSE(replay.Finished());
    const auto& tick_one = replay.CommandsForTick(1);
    CHECK(tick_one.size() == 1);
    CHECK(tick_one[0].scancode == 68);
    CHECK(replay.CommandsForTick(3).empty());

    soulknight::platform::InputSnapshot first;
    CHECK(replay.Next(&first));
    CHECK(first.move_x == doctest::Approx(0.5f));
    CHECK(first.aim_x == doctest::Approx(1.0f));
    CHECK(first.IsPressed(Action::Attack));
    CHECK(first.IsHeld(Action::Attack));
    CHECK_FALSE(replay.Finished());

    soulknight::platform::InputSnapshot second;
    CHECK(replay.Next(&second));
    CHECK(second.aim_y == doctest::Approx(1.0f));
    CHECK(second.IsReleased(Action::Attack));
    CHECK_FALSE(second.IsHeld(Action::Attack));
    CHECK(replay.Finished());
    CHECK_FALSE(replay.Next(&second));
}

TEST_CASE("Replay accepts weapon slot and interact actions" *
          doctest::test_suite("unit")) {
    TempReplayFile file(R"({
        "test_name":"two_weapon",
        "inputs":[
            {"tick":7,
             "pressed":["WeaponSlot2","Interact"],
             "released":[],"held":["WeaponSlot2"]}
        ]
    })");
    InteractiveReplay replay;
    std::string error;
    CHECK(replay.Load(file.Path(), &error));
    soulknight::platform::InputSnapshot snapshot;
    CHECK(replay.Next(&snapshot));
    CHECK(snapshot.IsPressed(Action::WeaponSlot2));
    CHECK(snapshot.IsPressed(Action::Interact));
    CHECK(snapshot.IsHeld(Action::WeaponSlot2));
}

TEST_CASE("Corrupt, empty and missing replay files are rejected" *
          doctest::test_suite("unit")) {
    TempReplayFile corrupt("{ not json");
    InteractiveReplay replay;
    std::string error;
    CHECK_FALSE(replay.Load(corrupt.Path(), &error));
    CHECK_FALSE(error.empty());

    TempReplayFile empty(R"({"inputs":[]})");
    CHECK_FALSE(replay.Load(empty.Path(), &error));

    CHECK_FALSE(replay.Load("/definitely/missing/replay.json", &error));
}

TEST_CASE("Replay skips unknown future action names without failure" *
          doctest::test_suite("unit")) {
    TempReplayFile file(R"({
        "inputs":[
            {"tick":1,"pressed":["FutureAction"],"released":[],"held":[]}
        ]
    })");
    InteractiveReplay replay;
    std::string error;
    CHECK(replay.Load(file.Path(), &error));
    soulknight::platform::InputSnapshot snapshot;
    CHECK(replay.Next(&snapshot));
    CHECK(snapshot.pressed.none());
}
