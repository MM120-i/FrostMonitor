#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "../include/frostmonitor/format.hpp"

using frostmonitor::formatCpuLine;
using frostmonitor::formatGpuLine;

TEST_CASE("formatCpuLine renders temperature and utilization", "[format]") {
    REQUIRE(formatCpuLine(74.5, 32) == "CPU 74.5°C | 32%");
    REQUIRE(formatCpuLine(100.0, 100) == "CPU 100.0°C | 100%");
}

TEST_CASE("formatCpuLine renders n/a when temperature is unavailable", "[format]") {
    REQUIRE(formatCpuLine(std::nullopt, 32) == "CPU n/a | 32%");
    REQUIRE(formatCpuLine(std::optional<double>{}, 0) == "CPU n/a | 0%");
}

TEST_CASE("formatCpuLine clamps utilization to 0..100", "[format]") {
    REQUIRE(formatCpuLine(50.0, -10) == "CPU 50.0°C | 0%");
    REQUIRE(formatCpuLine(50.0, 150) == "CPU 50.0°C | 100%");
}

TEST_CASE("formatGpuLine renders temperature and utilization", "[format]") {
    REQUIRE(formatGpuLine(61.25, 87) == "GPU 61.2°C | 87%");
}

TEST_CASE("formatGpuLine clamps utilization to 0..100", "[format]") {
    REQUIRE(formatGpuLine(30.0, -1) == "GPU 30.0°C | 0%");
    REQUIRE(formatGpuLine(30.0, 999) == "GPU 30.0°C | 100%");
}