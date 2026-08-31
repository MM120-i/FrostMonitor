#include <algorithm>
#include <format>

#include "../include/frostmonitor/format.hpp"

namespace frostmonitor {
    namespace {
        auto clampPercent(int value) -> int {
            return std::clamp(value, 0, 100);
        }
    }

    auto formatCpuLine(std::optional<double> tempC, int utilizationPct) -> std::string {
        const std::string temp = tempC.has_value()
            ? std::format("{:.1f}°C", *tempC)
            : "n/a";

        return std::format("CPU {} | {}%", temp, clampPercent(utilizationPct));
    }

    auto formatGpuLine(double tempC, int utilizationPct) -> std::string {
        return std::format("GPU {:.1f}°C | {}%", tempC, clampPercent(utilizationPct));
    }

    auto formatFpsLine(int fps) -> std::string {
        return std::format("FPS {}", std::clamp(fps, 0, 999));
    }
}