#include <catch2/catch_test_macros.hpp>

#include "../include/frostmonitor/fps.hpp"
#include "../include/frostmonitor/format.hpp"

TEST_CASE("formatFpsLine renders correctly") {
    CHECK(frostmonitor::formatFpsLine(60) == "FPS 60");
    CHECK(frostmonitor::formatFpsLine(144) == "FPS 144");
    CHECK(frostmonitor::formatFpsLine(0) == "FPS 0");
}

TEST_CASE("formatFpsLine clamps high values") {
    CHECK(frostmonitor::formatFpsLine(9999) == "FPS 999");
}

TEST_CASE("formatFpsLine clamps negative") {
    CHECK(frostmonitor::formatFpsLine(-5) == "FPS 0");
}

TEST_CASE("fps create fails gracefully when RTSS not running") {
    auto result = frostmonitor::FpsMonitor::create();

    if(!result)
        CHECK(result.error() == frostmonitor::FpsError::SharedMemoryOpenFailed);
}

TEST_CASE("struct layout matches expected sizes") {
    CHECK(sizeof(frostmonitor::RtssHeader) == 20);
    CHECK(sizeof(frostmonitor::RtssAppEntry) == 284);
}