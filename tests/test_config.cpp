#include <frostmonitor/config.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {
    class TempJsonFile {
    public:
        explicit TempJsonFile(std::string_view contents)
            : path_{std::filesystem::temp_directory_path() /
                    ("frostmonitor_test_" + std::to_string(counter_++) + ".json")}
        {
            std::ofstream out{path_};
            out << contents;
        }

        ~TempJsonFile(){
            std::error_code ec;
            std::filesystem::remove(path_, ec);
        }

        TempJsonFile(const TempJsonFile &) = delete;
        TempJsonFile& operator = (const TempJsonFile &) = delete;

        [[nodiscard]] const std::filesystem::path& path() const noexcept { 
            return path_; 
        }

    private:
        std::filesystem::path path_;
        inline static unsigned long long counter_{0};
    };
}  

TEST_CASE("loads a fully specified config"){
    const std::string json = R"({
        "app": { "name": "MyMonitor" },
        "polling_interval_ms": 500,
        "autostart": true,
        "gamesense": {
            "address": "127.0.0.1:59188",
            "discovery_file": "C:/custom/coreProps.json",
            "register_game": false
        },
        "events": {
            "cpu": { "name": "CPU_TEMP", "min": 0, "max": 110 },
            "gpu": { "name": "GPU_TEMP", "min": 0, "max": 110 }
        },
        "logging": { "level": "warn", "dir": "log", "max_bytes": 1048576, "max_files": 3 }
    })";

    const TempJsonFile file{json};
    const auto config = frostmonitor::loadConfig(file.path());

    REQUIRE(config.has_value());

    CHECK(config->pollingInterval == std::chrono::milliseconds{500});
    CHECK(config->autoStart == true);

    CHECK(config->gamesense.address == "127.0.0.1:59188");
    CHECK(config->gamesense.discoveryFile == "C:/custom/coreProps.json");
    CHECK(config->gamesense.registerGame == false);

    CHECK(config->cpuEvent.name == "CPU_TEMP");
    CHECK(config->cpuEvent.min == 0.0);
    CHECK(config->cpuEvent.max == 110.0);
    CHECK(config->gpuEvent.name == "GPU_TEMP");

    CHECK(config->logging.level == "warn");
    CHECK(config->logging.dir == "log");
    CHECK(config->logging.maxBytes == 1048576);
    CHECK(config->logging.maxFiles == 3);
}

TEST_CASE("missing file reports FileNotFound"){
    const auto missing = std::filesystem::temp_directory_path() / "frostmonitor_does_not_exist.json";
    const auto config = frostmonitor::loadConfig(missing);

    REQUIRE_FALSE(config.has_value());
    CHECK(config.error() == frostmonitor::ConfigError::FileNotFound);
}

TEST_CASE("malformed JSON reports ParseError"){
    const TempJsonFile file{"{ this is not valid json"};
    const auto config = frostmonitor::loadConfig(file.path());

    REQUIRE_FALSE(config.has_value());
    CHECK(config.error() == frostmonitor::ConfigError::ParseError);
}

TEST_CASE("empty object falls back to defaults"){
    const TempJsonFile file{"{}"};
    const auto config = frostmonitor::loadConfig(file.path());

    REQUIRE(config.has_value());

    CHECK(config->pollingInterval == std::chrono::milliseconds{1000});
    CHECK(config->autoStart == false);
    CHECK(config->gamesense.address.empty());
    CHECK(config->gamesense.registerGame == true);
    CHECK(config->cpuEvent.name == "CPU_STATS");
    CHECK(config->cpuEvent.min == 0.0);
    CHECK(config->cpuEvent.max == 100.0);
    CHECK(config->gpuEvent.name == "GPU_STATS");
    CHECK(config->logging.level == "debug");
    CHECK(config->logging.dir == "logs");
    CHECK(config->logging.maxBytes == 5 * 1024 * 1024);
    CHECK(config->logging.maxFiles == 5);
}

TEST_CASE("zero polling interval reports ValidationError"){
    const TempJsonFile file{R"({ "polling_interval_ms": 0 })"};
    const auto config = frostmonitor::loadConfig(file.path());

    REQUIRE_FALSE(config.has_value());
    CHECK(config.error() == frostmonitor::ConfigError::ValidationError);
}

TEST_CASE("negative polling interval reports ValidationError"){
    const TempJsonFile file{R"({ "polling_interval_ms": -500 })"};
    const auto config = frostmonitor::loadConfig(file.path());

    REQUIRE_FALSE(config.has_value());
    CHECK(config.error() == frostmonitor::ConfigError::ValidationError);
}

TEST_CASE("partial events keep defaults for missing fields"){
    const TempJsonFile file{R"({ "events": { "cpu": { "name": "MY_CPU" } } })"};
    const auto config = frostmonitor::loadConfig(file.path());

    REQUIRE(config.has_value());
    CHECK(config->cpuEvent.name == "MY_CPU");
    CHECK(config->cpuEvent.min == 0.0);
    CHECK(config->cpuEvent.max == 100.0);
    CHECK(config->gpuEvent.name == "GPU_STATS");
}

TEST_CASE("wrong-typed polling interval reports ParseError instead of crashing"){
    const TempJsonFile file{R"({ "polling_interval_ms": "fast" })"};
    const auto config = frostmonitor::loadConfig(file.path());

    REQUIRE_FALSE(config.has_value());
    CHECK(config.error() == frostmonitor::ConfigError::ParseError);
}

TEST_CASE("wrong-typed section reports ParseError instead of crashing"){
    const TempJsonFile file{R"({ "logging": 42 })"};
    const auto config = frostmonitor::loadConfig(file.path());

    REQUIRE_FALSE(config.has_value());
    CHECK(config.error() == frostmonitor::ConfigError::ParseError);
}
