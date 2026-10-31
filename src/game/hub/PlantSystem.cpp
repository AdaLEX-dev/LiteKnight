#include "PlantSystem.hpp"

#include <algorithm>

namespace soulknight::game::hub {

void PlantSystem::Clear() {
    plots_.clear();
    harvested_ = 0;
}

void PlantSystem::AddPlot(const std::string& id, const engine::Vec2& position,
                          float growth_time) {
    PlantPlot plot;
    plot.id = id;
    plot.position = position;
    plot.growth_time = std::max(0.1f, growth_time);
    plots_.push_back(std::move(plot));
}

void PlantSystem::Update(float delta_seconds) {
    if (delta_seconds <= 0.0f) return;
    for (PlantPlot& plot : plots_) {
        if (plot.state != PlantGrowthState::Growing) continue;
        plot.elapsed += delta_seconds;
        if (plot.elapsed >= plot.growth_time) {
            plot.elapsed = plot.growth_time;
            plot.state = PlantGrowthState::Ready;
        }
    }
}

int PlantSystem::NearestPlot(const engine::Vec2& position, float radius) const {
    if (radius <= 0.0f) return -1;
    int best = -1;
    float best_distance = radius * radius;
    for (std::size_t i = 0; i < plots_.size(); ++i) {
        const float distance =
            (plots_[i].position - position).LengthSquared();
        if (distance <= best_distance) {
            best = static_cast<int>(i);
            best_distance = distance;
        }
    }
    return best;
}

PlantInteraction PlantSystem::Interact(int index, const std::string& species,
                                       std::size_t harvest_amount) {
    if (index < 0 || static_cast<std::size_t>(index) >= plots_.size()) {
        return PlantInteraction::None;
    }
    PlantPlot& plot = plots_[static_cast<std::size_t>(index)];
    if (plot.state == PlantGrowthState::Empty) {
        plot.state = PlantGrowthState::Growing;
        plot.species = species.empty() ? "seed_dev" : species;
        plot.elapsed = 0.0f;
        return PlantInteraction::Planted;
    }
    if (plot.state == PlantGrowthState::Ready) {
        plot.state = PlantGrowthState::Empty;
        plot.species.clear();
        plot.elapsed = 0.0f;
        harvested_ += harvest_amount;
        return PlantInteraction::Harvested;
    }
    return PlantInteraction::NotReady;
}

std::size_t PlantSystem::ActiveCount() const {
    std::size_t count = 0;
    for (const PlantPlot& plot : plots_) {
        if (plot.state != PlantGrowthState::Empty) ++count;
    }
    return count;
}

void PlantSystem::Reset() {
    for (PlantPlot& plot : plots_) {
        plot.state = PlantGrowthState::Empty;
        plot.species.clear();
        plot.elapsed = 0.0f;
    }
    harvested_ = 0;
}

} // namespace soulknight::game::hub
