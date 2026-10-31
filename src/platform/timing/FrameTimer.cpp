// Soul Knight Linux — Frame Timer Implementation
#include "FrameTimer.hpp"
#include <chrono>
#include <algorithm>

namespace soulknight::platform {

FrameTimer::FrameTimer() {
    current_time_ = GetTimeSeconds();
}

void FrameTimer::BeginFrame() {
    double new_time = GetTimeSeconds();
    double frame_time = new_time - current_time_;
    current_time_ = new_time;

    // Apply time scale
    frame_time *= time_scale_;

    // Clamp spiral of death
    if (frame_time > MAX_ACCUMULATOR) {
        frame_time = MAX_ACCUMULATOR;
    }

    accumulator_ += frame_time;

    stats_.frame_time_ms = frame_time * 1000.0;
    stats_.sim_steps = 0;
}

void FrameTimer::EndFrame() {
    // Frame complete
    // Note: sim_steps incremented in ConsumeSimStep
}

void FrameTimer::Tick() {
    BeginFrame();

    // Fixed simulation steps
    while (ShouldSimulate()) {
        ConsumeSimStep();
    }

    EndFrame();
}

double FrameTimer::GetTimeSeconds() {
    using namespace std::chrono;
    // steady_clock is monotonic and must be used for frame simulation timing.
    const auto now = steady_clock::now();
    return duration<double>(now.time_since_epoch()).count();
}

} // namespace soulknight::platform