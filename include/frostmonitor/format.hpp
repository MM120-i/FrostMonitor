#pragma once

#include <optional>
#include <string>

namespace frostmonitor {
    [[nodiscard]] auto formatCpuLine(std::optional<double> tempC, int) -> std::string;
    [[nodiscard]] auto formatGpuLine(double, int) -> std::string;
}