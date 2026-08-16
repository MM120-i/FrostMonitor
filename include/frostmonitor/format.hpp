#pragma once

#include <optional>
#include <string>

namespace frostmonitor {
    [[nodiscard]] auto formatCpuLine(std::optional<double> tempC, int utilizationPct) -> std::string;
    [[nodiscard]] auto formatGpuLine(double tempC, int utilizationPct) -> std::string;
}