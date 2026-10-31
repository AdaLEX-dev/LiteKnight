// Soul Knight Linux — reference timing regression tests
#include <doctest/doctest.h>
#include "platform/timing/FrameTimer.hpp"

using soulknight::platform::FrameTimer;

TEST_CASE("FrameTimer uses confirmed 50 Hz reference fixed step" * doctest::test_suite("unit")) {
    CHECK(FrameTimer::TARGET_DT == doctest::Approx(0.02));
    CHECK(FrameTimer::TARGET_DT != doctest::Approx(1.0 / 60.0));
    CHECK(FrameTimer::MAX_ACCUMULATOR >= FrameTimer::TARGET_DT);
}
