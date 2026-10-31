// Soul Knight Linux — Frame Timer
#pragma once

#include <cstdint>
#include <chrono>
#include <engine/core/Types.hpp>

namespace soulknight::platform {

class FrameTimer {
public:
    // CONFIRMED reference fixed timestep: serialized Unity TimeManager
    // Fixed_Timestep = 0.019999999552965164 s (50 Hz). The render target remains
    // 60 FPS; do not confuse the two.
    static constexpr double TARGET_DT = 1.0 / 50.0;       // 20 ms / 50 Hz
    static constexpr double MAX_ACCUMULATOR = 0.3333333333; // reference max allowed timestep

    FrameTimer();

    void BeginFrame();
    void EndFrame();
    void Tick(); // Convenience: BeginFrame + EndFrame

    // Fixed timestep interface
    bool ShouldSimulate() const { return accumulator_ >= TARGET_DT; }
    double GetFixedDeltaTime() const { return TARGET_DT; }
    void ConsumeSimStep() { accumulator_ -= TARGET_DT; sim_steps_++; }

    // Interpolation
    double GetInterpolationAlpha() const {
        return accumulator_ / TARGET_DT;
    }

    // Stats
    struct Stats {
        double frame_time_ms = 0.0;
        double sim_time_ms = 0.0;
        double render_time_ms = 0.0;
        int sim_steps = 0;
    };
    const Stats& GetStats() const { return stats_; }

    // Time control (for testing)
    void SetTimeScale(double scale) { time_scale_ = scale; }
    double GetTimeScale() const { return time_scale_; }
    void SetTime(double time) { current_time_ = time; }
    double GetTime() const { return current_time_; }

private:
    double current_time_ = 0.0;
    double accumulator_ = 0.0;
    double time_scale_ = 1.0;

    int sim_steps_ = 0;

    Stats stats_;

    static double GetTimeSeconds();
};

} // namespace soulknight::platform