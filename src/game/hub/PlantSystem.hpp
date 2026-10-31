// Soul Knight Linux — Living Room plant/garden foundation
#pragma once

#include "engine/math/Vec2.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace soulknight::game::hub {

// Reference growth values, seed economy and harvest tables are UNRECOVERED.
// This is a clearly marked development foundation with deterministic timing.
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
    float growth_time = 5.0f;
    float elapsed = 0.0f;
};

enum class PlantInteraction {
    None,
    Planted,
    Harvested,
    NotReady
};

class PlantSystem {
public:
    void Clear();
    void AddPlot(const std::string& id, const engine::Vec2& position,
                 float growth_time = 5.0f);
    void Update(float delta_seconds);

    // Nearest plot within radius. Negative index means none.
    int NearestPlot(const engine::Vec2& position, float radius) const;
    PlantInteraction Interact(int index, const std::string& species,
                              std::size_t harvest_amount = 1);

    const std::vector<PlantPlot>& Plots() const { return plots_; }
    std::size_t ActiveCount() const;
    std::size_t HarvestedCount() const { return harvested_; }
    void Reset();

private:
    std::vector<PlantPlot> plots_;
    std::size_t harvested_ = 0;
};

} // namespace soulknight::game::hub
