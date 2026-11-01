// Soul Knight Linux — Living Room planter/garden state
#pragma once

#include "engine/math/Vec2.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace soulknight::game::hub {

// The plant prefabs and harvest payloads are recovered from the common bundle.
// PlantInfo/Resources (authoritative growth timings) is not present in the
// extracted APK, so growth_seconds is a documented native reconstruction.
enum class PlantGrowthState {
    Empty,
    Growing,
    Ready
};

struct PlantPlot {
    std::string id;
    engine::Vec2 position{0.0f, 0.0f};
    PlantGrowthState state = PlantGrowthState::Empty;
    std::string species;
    std::string harvest_item;
    float growth_time = 30.0f;
    float elapsed = 0.0f;
    bool watered = false;
    bool locked = false;
    int stages = 2;
    int yield_count = 1;
};

enum class PlantInteraction {
    None,
    Locked,
    NoSeed,
    Planted,
    Watered,
    Harvested,
    NotReady
};

class PlantSystem {
public:
    void Clear();
    void AddPlot(const std::string& id, const engine::Vec2& position,
                 float growth_time = 30.0f, bool locked = false,
                 int stages = 2);
    void Update(float delta_seconds);

    // Seed inventory recovered seed ids (plant_*), counts are native state.
    void AddSeed(const std::string& species, int count = 1);
    void SetSeed(const std::string& species, int count);
    void RemoveSeed(const std::string& species);
    int SeedCount(const std::string& species) const;
    std::string FirstAvailableSeed() const;
    const std::map<std::string, int>& Seeds() const { return seeds_; }
    void ClearSeeds() { seeds_.clear(); }

    // Nearest plot within radius. Negative index means none.
    int NearestPlot(const engine::Vec2& position, float radius) const;
    PlantInteraction Interact(int index, const std::string& species,
                              float growth_seconds = 0.0f,
                              int stages = 0,
                              const std::string& harvest_item = {},
                              int yield_count = 1);

    const std::vector<PlantPlot>& Plots() const { return plots_; }
    std::size_t ActiveCount() const;
    std::size_t HarvestedCount() const { return harvested_; }
    void Reset();

    // Stable, compact persistence records. They deliberately avoid JSON so
    // SaveSerializer remains the only JSON authority.
    std::vector<std::string> SerializePlots() const;
    std::vector<std::string> SerializeSeeds() const;
    bool RestorePlots(const std::vector<std::string>& records);
    bool RestoreSeeds(const std::vector<std::string>& records);

private:
    std::vector<PlantPlot> plots_;
    std::map<std::string, int> seeds_;
    std::size_t harvested_ = 0;
};

} // namespace soulknight::game::hub
