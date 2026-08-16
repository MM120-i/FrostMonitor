#include "../include/frostmonitor/config.hpp"

#include <nlohmann/json.hpp>
#include <fstream>

namespace frostmonitor {
    namespace {
        template<typename T>
        auto require(const nlohmann::json &j, std::string_view key) -> std::expected<T, ConfigError>{
            if(!j.contains(key))
                return std::unexpected(ConfigError::ValidationError);

            return j.at(key).get<T>();
        }
    }

    auto loadConfig(const std::filesystem::path &path) -> std::expected<Config, ConfigError> {
        std::ifstream file{path};

        if(!file)
            return std::unexpected(ConfigError::FileNotFound);

        nlohmann::json root;

        try{
            file >> root;

            Config cfg;

            auto app = require<nlohmann::json>(root, "app");

            if(app)
                cfg.appName = app->value("name", cfg.appName);

            cfg.pollingInterval = std::chrono::milliseconds(root.value("polling_interval_ms", 1000));

            if(cfg.pollingInterval <= std::chrono::milliseconds::zero())
                return std::unexpected(ConfigError::ValidationError);

            cfg.autoStart = root.value("autostart", false);

            auto gameSense = require<nlohmann::json>(root, "gamesense");

            if(gameSense){
                cfg.gamesense.address = gameSense->value("address", cfg.gamesense.discoveryFile.string());
                cfg.gamesense.discoveryFile = gameSense->value("discovery_file", cfg.gamesense.discoveryFile.string());
                cfg.gamesense.registerGame = gameSense->value("register_game", true);
            }

            auto events = require<nlohmann::json>(root, "events");

            if(events){
                auto cpu = require<nlohmann::json>(*events, "cpu");

                if(cpu){
                    cfg.cpuEvent.name = cpu->value("name", cfg.cpuEvent.name);
                    cfg.cpuEvent.min = cpu->value("min", cfg.cpuEvent.min);
                    cfg.cpuEvent.max = cpu->value("max", cfg.cpuEvent.max);
                }

                auto gpu = require<nlohmann::json>(*events, "gpu");

                if(gpu){
                    cfg.gpuEvent.name = gpu->value("name", cfg.gpuEvent.name);
                    cfg.gpuEvent.min = gpu->value("min", cfg.gpuEvent.min);
                    cfg.gpuEvent.max = gpu->value("max", cfg.gpuEvent.max);
                }
            }

            auto logging = require<nlohmann::json>(root, "logging");

            if(logging){
                cfg.logging.level = logging->value("level", cfg.logging.level);
                cfg.logging.dir = logging->value("dir", cfg.logging.dir.string());
                cfg.logging.maxBytes = logging->value("max_bytes", cfg.logging.maxBytes);
                cfg.logging.maxFiles = logging->value("max_files", cfg.logging.maxFiles);
            }

            return cfg;
        }
        catch(const nlohmann::json::exception &){
            return std::unexpected(ConfigError::ParseError);
        }
    }
}