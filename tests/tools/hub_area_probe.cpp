// Soul Knight Linux — area-based Living Room collision/geometry probe
//
// Usage:
//   soul-knight-geometry --area living-room
//   soul-knight-geometry --area garden
//   soul-knight-geometry --area trader
//   soul-knight-geometry --area mech
//   soul-knight-geometry --area reforging
//   soul-knight-geometry --area workshop
//   soul-knight-geometry --area planting
//   soul-knight-geometry --area living-room --scene analysis/assets/scenes/hero_room.json
//   soul-knight-geometry --area x,y,w,h
//
// This is deliberately not a "solid exists" check. It simulates the player's
// AABB from every side, including diagonal/corner approaches, and verifies the
// final position stays outside solid props or inside the recovered floor
// footprint. Failures print the AREA/OBJECT/POSITION/BOUNDARY/COLLISION report
// requested by the QA contract.
#include "engine/collision/AabbCollision.hpp"
#include "engine/rendering/WorldDepth.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

namespace {

using Json = nlohmann::json;
using soulknight::engine::Vec2;
using soulknight::engine::collision::Aabb;
using soulknight::engine::collision::AabbCollisionWorld;

struct Area {
    std::string name;
    Aabb bounds{};
    bool solid_object = false;
};

struct Box {
    Aabb box{};
    std::string path;
};

struct SceneData {
    Aabb bounds{};
    std::vector<Box> solids;
    std::set<std::pair<int, int>> floor_cells;
    std::set<std::pair<int, int>> allowed_cells;
    Json instances;
    std::vector<std::string> failures;
};

int failures = 0;

Aabb ReadAabb(const Json& value) {
    Aabb box;
    if (value.is_array() && value.size() == 4) {
        box.min = {value[0].get<float>(), value[1].get<float>()};
        box.max = {value[0].get<float>() + value[2].get<float>(),
                   value[1].get<float>() + value[3].get<float>()};
    }
    return box;
}

bool ParseRect(const std::string& text, Aabb* out) {
    std::array<float, 4> values{};
    std::size_t index = 0;
    std::stringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ',') && index < values.size()) {
        const char* begin = token.c_str();
        char* end = nullptr;
        const float value = std::strtof(begin, &end);
        if (end == begin || *end != '\0' || !std::isfinite(value)) {
            return false;
        }
        values[index++] = value;
    }
    if (index != values.size()) return false;
    out->min = {values[0], values[1]};
    out->max = {values[0] + values[2], values[1] + values[3]};
    return out->IsValid();
}

bool PointInAabb(const Vec2& point, const Aabb& box) {
    return point.x >= box.min.x && point.x <= box.max.x &&
           point.y >= box.min.y && point.y <= box.max.y;
}

bool FootOnFloor(const SceneData& scene, const Vec2& foot,
                 float tolerance = 0.0f, bool include_aisles = false) {
    const auto covered = [&](const std::pair<int, int>& cell) {
        const Vec2 min{static_cast<float>(cell.first) - tolerance,
                       static_cast<float>(cell.second) - tolerance};
        const Vec2 max{static_cast<float>(cell.first) + 1.0f + tolerance,
                       static_cast<float>(cell.second) + 1.0f + tolerance};
        return foot.x >= min.x && foot.x <= max.x &&
               foot.y >= min.y && foot.y <= max.y;
    };
    const int x = static_cast<int>(std::floor(foot.x));
    const int y = static_cast<int>(std::floor(foot.y));
    const auto& cells = include_aisles ? scene.allowed_cells
                                       : scene.floor_cells;
    if (!tolerance) return cells.contains({x, y});
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (cells.contains({x + dx, y + dy}) &&
                covered({x + dx, y + dy})) {
                return true;
            }
        }
    }
    return false;
}

bool PlayerFullySupported(const SceneData& scene, const Vec2& center) {
    const float foot_y = center.y - 0.5f;
    const float xs[] = {center.x - 0.34f, center.x, center.x + 0.34f};
    const float ys[] = {foot_y, foot_y + 0.34f};
    for (float x : xs) {
        for (float y : ys) {
            if (!FootOnFloor(scene, {x, y}, 0.0f, true)) return false;
        }
    }
    return true;
}

void Report(const std::string& area, const std::string& object,
            const Vec2& start, const Vec2& expected, const Vec2& actual,
            bool blocked_x, bool blocked_y, const std::string& depth) {
    ++failures;
    std::cerr << "AREA: " << area << "\n"
              << "OBJECT: " << object << "\n"
              << "PLAYER POSITION: (" << start.x << ", " << start.y << ")\n"
              << "EXPECTED BOUNDARY: " << expected.x << ", " << expected.y
              << "\n"
              << "ACTUAL POSITION: (" << actual.x << ", " << actual.y << ")\n"
              << "COLLISION RESULT: blocked_x=" << blocked_x
              << " blocked_y=" << blocked_y << "\n"
              << "RENDER/DEPTH RESULT: " << depth << "\n\n";
}

SceneData LoadScene(const std::string& path) {
    SceneData scene;
    std::ifstream stream(path);
    if (!stream) {
        std::cerr << "scene not found: " << path << "\n";
        return scene;
    }
    Json document = Json::parse(stream, nullptr, false);
    if (document.is_discarded()) return scene;
    scene.bounds = ReadAabb(document["collision"]["bounds"]);
    for (const Json& solid : document["collision"]["solids"]) {
        if (!solid.is_array() || solid.size() != 4) continue;
        Box box;
        box.box = {{solid[0].get<float>(), solid[1].get<float>()},
                   {solid[2].get<float>(), solid[3].get<float>()}};
        scene.solids.push_back(std::move(box));
    }
    for (const Json& instance : document["instances"]) {
        const std::string instance_path = instance.value("path", "");
        if (instance_path.find("room_tilemap/floor_") != std::string::npos) {
            const auto& position = instance["position"];
            scene.floor_cells.insert({
                static_cast<int>(std::floor(position[0].get<float>())),
                static_cast<int>(std::floor(position[1].get<float>()))});
        }
    }
    const auto read_cells = [&](const char* key) {
        if (!document.contains("topology") ||
            !document["topology"].contains(key) ||
            !document["topology"][key].is_array()) {
            return;
        }
        for (const Json& cell : document["topology"][key]) {
            if (!cell.is_array() || cell.size() != 2) continue;
            scene.floor_cells.insert({cell[0].get<int>(),
                                      cell[1].get<int>()});
        }
    };
    read_cells("floor_cells");
    scene.allowed_cells = scene.floor_cells;
    if (document.contains("topology") &&
        document["topology"].contains("aisle_cells") &&
        document["topology"]["aisle_cells"].is_array()) {
        for (const Json& cell : document["topology"]["aisle_cells"]) {
            if (!cell.is_array() || cell.size() != 2) continue;
            scene.allowed_cells.insert({cell[0].get<int>(),
                                        cell[1].get<int>()});
        }
    }
    scene.instances = document["instances"];
    return scene;
}

AabbCollisionWorld BuildWorld(const SceneData& scene) {
    AabbCollisionWorld world;
    world.SetBounds(scene.bounds);
    for (const Box& solid : scene.solids) world.AddSolid(solid.box);
    return world;
}

std::vector<Area> NamedAreas() {
    return {
        {"living-room", {{-63.0f, -28.0f}, {49.0f, 15.0f}}, false},
        {"garden", {{-63.0f, -19.0f}, {-15.0f, 15.0f}}, false},
        {"trader", {{23.10f, 3.35f}, {32.04f, 7.00f}}, true},
        {"mech", {{41.14f, 4.34f}, {47.64f, 8.84f}}, true},
        {"reforging", {{33.49f, -6.42f}, {36.91f, -3.03f}}, true},
        {"weapon-station", {{43.14f, -6.30f}, {46.82f, -3.03f}}, true},
        {"material-chest", {{47.30f, -0.17f}, {50.00f, 3.57f}}, true},
        {"workshop", {{16.0f, -10.0f}, {49.0f, 7.0f}}, false},
        {"planting", {{-50.0f, -6.0f}, {-23.0f, 5.0f}}, false},
    };
}

void ProbeSolid(AabbCollisionWorld& world, const SceneData& scene,
                const Area& area) {
    const Aabb& box = area.bounds;
    const float pad = 1.2f;
    const Vec2 sides[] = {
        {box.min.x - pad, (box.min.y + box.max.y) * 0.5f},
        {box.max.x + pad, (box.min.y + box.max.y) * 0.5f},
        {(box.min.x + box.max.x) * 0.5f, box.min.y - pad},
        {(box.min.x + box.max.x) * 0.5f, box.max.y + pad},
        {box.min.x - pad, box.min.y - pad},
        {box.max.x + pad, box.min.y - pad},
        {box.min.x - pad, box.max.y + pad},
        {box.max.x + pad, box.max.y + pad},
    };
    for (const Vec2& start : sides) {
        const Vec2 target{(box.min.x + box.max.x) * 0.5f,
                          (box.min.y + box.max.y) * 0.5f};
        Vec2 out = start;
        const auto result = world.Move(start, {0.4f, 0.4f},
                                       target - start, out);
        if (PointInAabb(out, box)) {
            Report(area.name, "recovered solid", start,
                   {box.min.x, box.min.y}, out, result.blocked_x,
                   result.blocked_y, "solid collision penetrated");
        }
    }
    const std::string depth =
        std::isfinite(scene.bounds.min.x) ? "finite" : "invalid";
    if (depth != "finite") {
        Report(area.name, "scene bounds", {}, {}, {}, false, false, depth);
    }
}

void ProbeRegion(AabbCollisionWorld& world, const SceneData& scene,
                 const Area& area) {
    constexpr int kRays = 16;
    const Vec2 center{(area.bounds.min.x + area.bounds.max.x) * 0.5f,
                      (area.bounds.min.y + area.bounds.max.y) * 0.5f};
    int tested = 0;
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 9; ++x) {
            const Vec2 start{
                area.bounds.min.x +
                    (area.bounds.max.x - area.bounds.min.x) *
                        (static_cast<float>(x) + 0.5f) / 9.0f,
                area.bounds.min.y +
                    (area.bounds.max.y - area.bounds.min.y) *
                        (static_cast<float>(y) + 0.5f) / 7.0f};
            if (!PointInAabb(start, area.bounds) ||
                !PlayerFullySupported(scene, start)) {
                continue;
            }
            bool blocked = false;
            for (const Box& solid : scene.solids) {
                if (solid.box.Intersects(
                        {{start.x - 0.4f, start.y - 0.4f},
                         {start.x + 0.4f, start.y + 0.4f}})) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) continue;
            ++tested;
            for (int ray = 0; ray < kRays; ++ray) {
                const float angle = static_cast<float>(ray) *
                    6.283185307179586f / static_cast<float>(kRays);
                const Vec2 delta{std::cos(angle) * 80.0f,
                                 std::sin(angle) * 80.0f};
                Vec2 out = start;
                const auto result = world.Move(start, {0.4f, 0.4f}, delta,
                                               out);
                const Vec2 foot{out.x, out.y - 0.5f};
                if (!FootOnFloor(scene, foot, 0.45f, true) ||
                    !PointInAabb(out, scene.bounds)) {
                    Report(area.name, "floor boundary", start,
                           {scene.bounds.min.x, scene.bounds.min.y}, out,
                           result.blocked_x, result.blocked_y,
                           "escaped recovered floor union");
                }
            }
        }
    }
    if (tested == 0) {
        std::cerr << "AREA: " << area.name
                  << "\nOBJECT: probe start\nPLAYER POSITION: ("
                  << center.x << ", " << center.y
                  << ")\nEXPECTED BOUNDARY: at least one walkable start\n"
                  << "ACTUAL POSITION: no walkable sample\n"
                  << "COLLISION RESULT: untested\n"
                  << "RENDER/DEPTH RESULT: untested\n\n";
        ++failures;
    }
}

void ProbeDepth(const SceneData& scene, const Area& area) {
    for (const Json& instance : scene.instances) {
        if (!instance.value("ground_depth", false)) continue;
        const auto& position = instance["position"];
        const Vec2 pos{position[0].get<float>(), position[1].get<float>()};
        if (!PointInAabb(pos, area.bounds)) continue;
        const float order =
            static_cast<float>(soulknight::engine::rendering::WorldDepthOrder(
                pos.y));
        if (!std::isfinite(order)) {
            Report(area.name, instance.value("path", "instance"), pos,
                   {pos.x, pos.y}, pos, false, false, "non-finite depth");
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string area_name = "living-room";
    std::string scene_path = "analysis/assets/scenes/hero_room.json";
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if ((argument == "--area" || argument == "--review-area") &&
            i + 1 < argc) {
            area_name = argv[++i];
        } else if (argument.rfind("--area=", 0) == 0) {
            area_name = argument.substr(7);
        } else if (argument == "--scene" && i + 1 < argc) {
            scene_path = argv[++i];
        }
    }
    if (area_name == "coordinate-regression" ||
        area_name == "rogue" || area_name == "mission-board" ||
        area_name == "portal" || area_name == "grave") {
        const std::filesystem::path root =
            std::filesystem::path(scene_path).parent_path().parent_path()
                .parent_path().parent_path();
        const std::filesystem::path script =
            root / "tools/qa/living_room_coordinate_regression.py";
        const std::string command =
            "python3 \"" + script.string() + "\"";
        const int result = std::system(command.c_str());
        return result == 0 ? 0 : 1;
    }
    Area area;
    bool dungeon_area = false;
    if (area_name == "dungeon-1-1" || area_name == "dungeon-room-15x15") {
        scene_path = "analysis/assets/scenes/r1_1_visual.json";
        dungeon_area = true;
    } else if (area_name == "dungeon-1-2") {
        scene_path = "analysis/assets/scenes/r1_2_visual.json";
        dungeon_area = true;
    } else if (area_name == "dungeon-room-15x21") {
        scene_path = "analysis/assets/scenes/pattern/r1_41_visual.json";
        dungeon_area = true;
    } else if (area_name == "dungeon-room-21x15") {
        scene_path = "analysis/assets/scenes/pattern/r1_64_visual.json";
        dungeon_area = true;
    } else if (area_name == "dungeon-room-21x21") {
        scene_path = "analysis/assets/scenes/pattern/r1_86_visual.json";
        dungeon_area = true;
    } else if (area_name == "boss-room" ||
               area_name == "boss-room-r1") {
        scene_path = "analysis/assets/scenes/r1_boss1_visual.json";
        dungeon_area = true;
    } else if (area_name == "boss-room-r2") {
        scene_path = "analysis/assets/scenes/r2_boss1_visual.json";
        dungeon_area = true;
    } else if (area_name == "boss-room-r3") {
        scene_path = "analysis/assets/scenes/r3_boss2_visual.json";
        dungeon_area = true;
    } else if (area_name == "special-sell") {
        scene_path = "analysis/assets/scenes/r_sell_visual.json";
        dungeon_area = true;
    } else if (area_name == "special-statue") {
        scene_path = "analysis/assets/scenes/r_statue_visual.json";
        dungeon_area = true;
    } else if (area_name == "special-mineral") {
        scene_path = "analysis/assets/scenes/r_mineral_visual.json";
        dungeon_area = true;
    } else if (area_name == "special-mercenary") {
        scene_path = "analysis/assets/scenes/r_mercenary_visual.json";
        dungeon_area = true;
    } else if (area_name == "special-mount") {
        scene_path = "analysis/assets/scenes/r_mount_visual.json";
        dungeon_area = true;
    } else if (area_name == "special-chest") {
        scene_path = "analysis/assets/scenes/r_chest_visual.json";
        dungeon_area = true;
    }
    const std::vector<Area> named = NamedAreas();
    const auto found = std::find_if(
        named.begin(), named.end(),
        [&](const Area& candidate) { return candidate.name == area_name; });
    if (dungeon_area) {
        area.name = area_name;
    } else if (found != named.end()) {
        area = *found;
    } else if (!ParseRect(area_name, &area.bounds)) {
        std::cerr << "unknown area: " << area_name
                  << "\nuse living-room|garden|trader|mech|reforging|"
                     "workshop|planting or x,y,w,h\n";
        return 2;
    } else {
        area.name = "rect " + area_name;
        // A precise rectangle is always a playable-region probe: every point
        // in it must remain on the recovered floor and inside the collision
        // boundary. Named object areas above select solid-object probing.
        area.solid_object = false;
    }
    const SceneData scene = LoadScene(scene_path);
    if (dungeon_area) {
        float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
        for (const auto& cell : scene.floor_cells) {
            min_x = std::min(min_x, static_cast<float>(cell.first));
            min_y = std::min(min_y, static_cast<float>(cell.second));
            max_x = std::max(max_x, static_cast<float>(cell.first) + 1.0f);
            max_y = std::max(max_y, static_cast<float>(cell.second) + 1.0f);
        }
        if (max_x > min_x && max_y > min_y) {
            area.bounds = {{min_x, min_y}, {max_x, max_y}};
        }
    }
    if (scene.solids.empty() || !scene.bounds.IsValid()) {
        std::cerr << "scene has no collision topology: " << scene_path
                  << "\n";
        return 2;
    }
    AabbCollisionWorld world = BuildWorld(scene);
    if (area.solid_object) {
        ProbeSolid(world, scene, area);
    } else {
        ProbeRegion(world, scene, area);
    }
    ProbeDepth(scene, area);
    std::cout << "AREA: " << area.name << "\n"
              << "SCENE: " << scene_path << "\n"
              << "SOLIDS: " << scene.solids.size() << "\n"
              << "RESULT: " << (failures == 0 ? "PASS" : "FAIL") << "\n";
    return failures == 0 ? 0 : 1;
}
