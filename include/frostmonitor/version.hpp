#pragma once

#include <string_view>

namespace frostmonitor {    
    #ifndef FROSTMON_VERSION
        #define FROSTMON_VERSION "0.0.0-dev" // NOLINT(cppcoreguidelines-macro-usage)
    #endif

    enum class ExitCode: std::uint8_t {
        SUCCESS = 0,
        CONFIG_ERROR = 1,
        SENSOR_ERROR = 2,
    };

    inline constexpr std::string_view appName{"FrostMonitor"};
    inline constexpr std::string_view appVersion{FROSTMON_VERSION};
}