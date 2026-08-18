#pragma once

#include <string_view>

namespace frostmonitor {
    #ifndef FROSTMON_VERSION
        #define FROSTMON_VERSION "0.0.0-dev" // NOLINT(cppcoreguidelines-macro-usage)
    #endif

    inline constexpr std::string_view appName{"FrostMonitor"};
    inline constexpr std::string_view appVersion{FROSTMON_VERSION};
}