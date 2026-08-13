#pragma once

#include <chrono>
#include <expected>
#include <filesystem>
#include <string>

namespace frostmonitor {
    struct GamesenseConfig {
        std::string address;
        std::filesystem::path discoveryFile{"C:/ProgramData/SteelSeries/SteelSeries Engine 3/coreProps.json"};
        bool registerGame{true};
    };

    struct EventConfig {
        std::string name;
        double min{0.0};
        double max{100.0};
    };

    struct LoggingConfig {
        std::string level{"debug"};
        std::filesystem::path dir{"logs"};
        std::size_t maxBytes{5 * 1024 * 1024};
        std::size_t maxFiles{5};
    };

    struct Config {
        std::string appName{"FrostMonitor"};
        std::chrono::milliseconds pollingInterval{1000};
        bool autoStart{false};
        GamesenseConfig gamesense;
        EventConfig cpuEvent{"CPU_STATS", 0.0, 100.0};
        EventConfig gpuEvent{"GPU_STATS", 0.0, 100.0};
        LoggingConfig logging;
    };

    enum class ConfigError {
        FileNotFound,
        ParseError,
        ValidationError,
    };

    auto loadConfig(const std::filesystem::path &) -> std::expected<Config, ConfigError>;
};