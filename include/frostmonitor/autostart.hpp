#pragma once

#include <filesystem>
#include <string>

namespace frostmonitor {
    constexpr std::wstring_view kAutostartValueName = L"FrostMonitor";
    using path = std::filesystem::path;

    [[nodiscard]] std::wstring buildAutoStartCommand(const path &exePath, const path &configPath);
    [[nodiscard]] auto currentExePath() -> path;
    bool syncAutoStart(bool enabled, const path &exePath, const path &configPath);
}