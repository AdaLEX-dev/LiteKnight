#include "PlantSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace soulknight::game::hub {
namespace {

std::vector<std::string> Split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    std::istringstream stream(value);
    while (std::getline(stream, current, delimiter)) parts.push_back(current);
    return parts;
}

} // namespace

void PlantSystem::Clear() {
    plots_.clear();
    seeds_.clear();
    harvested_ = 0;
}

void PlantSystem::AddPlot(const std::string& id, const engine::Vec2& position,
                          float growth_time, bool locked, int stages) {
    PlantPlot plot;
    plot.id = id;
    plot.position = position;
    plot.growth_time = std::max(0.1f, growth_time);
    plot.locked = locked;
    plot.stages = std::max(2, stages);
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

void PlantSystem::AddSeed(const std::string& species, int count) {
    if (species.empty() || count <= 0) return;
    seeds_[species] += count;
}

void PlantSystem::SetSeed(const std::string& species, int count) {
    if (species.empty()) return;
    if (count <= 0) {
        seeds_.erase(species);
    } else {
        seeds_[species] = count;
    }
}

void PlantSystem::RemoveSeed(const std::string& species) {
    seeds_.erase(species);
}

int PlantSystem::SeedCount(const std::string& species) const {
    const auto found = seeds_.find(species);
    return found == seeds_.end() ? 0 : found->second;
}

std::string PlantSystem::FirstAvailableSeed() const {
    for (const auto& [species, count] : seeds_) {
        if (count > 0) return species;
    }
    return {};
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
                                       float growth_seconds, int stages,
                                       const std::string& harvest_item,
                                       int yield_count) {
    if (index < 0 || static_cast<std::size_t>(index) >= plots_.size()) {
        return PlantInteraction::None;
    }
    PlantPlot& plot = plots_[static_cast<std::size_t>(index)];
    if (plot.locked) return PlantInteraction::Locked;
    if (plot.state == PlantGrowthState::Empty) {
        if (species.empty() || SeedCount(species) <= 0) {
            return PlantInteraction::NoSeed;
        }
        seeds_[species] -= 1;
        if (seeds_[species] <= 0) seeds_.erase(species);
        plot.state = PlantGrowthState::Growing;
        plot.species = species;
        plot.harvest_item = harvest_item;
        if (growth_seconds > 0.0f) {
            plot.growth_time = std::max(0.1f, growth_seconds);
        }
        if (stages >= 2) plot.stages = stages;
        plot.yield_count = std::max(1, yield_count);
        plot.elapsed = 0.0f;
        plot.watered = false;
        return PlantInteraction::Planted;
    }
    if (plot.state == PlantGrowthState::Ready) {
        plot.state = PlantGrowthState::Empty;
        plot.species.clear();
        plot.harvest_item.clear();
        plot.elapsed = 0.0f;
        plot.watered = false;
        plot.yield_count = 1;
        ++harvested_;
        return PlantInteraction::Harvested;
    }
    if (!plot.watered) {
        plot.watered = true;
        // Native reconstruction: watering halves the remaining timer. The
        // authoritative water/growth curve is not in the extracted assets.
        const float remaining = std::max(0.0f, plot.growth_time - plot.elapsed);
        plot.elapsed = std::min(plot.growth_time,
                                plot.elapsed + remaining * 0.5f);
        return PlantInteraction::Watered;
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
        plot.harvest_item.clear();
        plot.elapsed = 0.0f;
        plot.watered = false;
        plot.yield_count = 1;
    }
    harvested_ = 0;
}

std::vector<std::string> PlantSystem::SerializePlots() const {
    std::vector<std::string> records;
    for (const PlantPlot& plot : plots_) {
        std::ostringstream out;
        out << plot.id << '|' << plot.species << '|'
            << static_cast<int>(plot.state) << '|' << plot.elapsed << '|'
            << (plot.watered ? 1 : 0) << '|' << (plot.locked ? 1 : 0) << '|'
            << plot.growth_time << '|' << plot.stages << '|'
            << plot.harvest_item << '|' << plot.yield_count;
        records.push_back(out.str());
    }
    return records;
}

std::vector<std::string> PlantSystem::SerializeSeeds() const {
    std::vector<std::string> records;
    for (const auto& [species, count] : seeds_) {
        records.push_back(species + "|" + std::to_string(count));
    }
    return records;
}

bool PlantSystem::RestorePlots(const std::vector<std::string>& records) {
    if (records.empty()) return false;
    std::map<std::string, PlantPlot> restored;
    for (const std::string& record : records) {
        const std::vector<std::string> parts = Split(record, '|');
        if (parts.size() != 10 || parts[0].empty()) continue;
        PlantPlot plot;
        plot.id = parts[0];
        plot.species = parts[1];
        const int state = std::clamp(std::atoi(parts[2].c_str()), 0, 2);
        plot.state = static_cast<PlantGrowthState>(state);
        plot.elapsed = std::clamp(
            static_cast<float>(std::atof(parts[3].c_str())),
            0.0f, 1.0e6f);
        plot.watered = parts[4] == "1";
        plot.locked = parts[5] == "1";
        plot.growth_time = std::max(0.1f,
            static_cast<float>(std::atof(parts[6].c_str())));
        plot.stages = std::max(2, std::atoi(parts[7].c_str()));
        plot.harvest_item = parts[8];
        plot.yield_count = std::max(1, std::atoi(parts[9].c_str()));
        restored[plot.id] = std::move(plot);
    }
    if (restored.empty()) return false;
    for (PlantPlot& plot : plots_) {
        const auto found = restored.find(plot.id);
        if (found == restored.end()) continue;
        const bool locked = plot.locked;
        plot = found->second;
        // Locked status comes from the recovered planter prefab, not save data.
        plot.locked = locked;
        if (plot.state == PlantGrowthState::Growing) {
            plot.elapsed = std::min(plot.elapsed, plot.growth_time);
        }
    }
    return true;
}

bool PlantSystem::RestoreSeeds(const std::vector<std::string>& records) {
    seeds_.clear();
    for (const std::string& record : records) {
        const std::vector<std::string> parts = Split(record, '|');
        if (parts.size() != 2 || parts[0].empty()) continue;
        const int count = std::max(0, std::atoi(parts[1].c_str()));
        if (count > 0) seeds_[parts[0]] = count;
    }
    return true;
}

} // namespace soulknight::game::hub
