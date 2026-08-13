#include <thread>
#include <memory>
#include <filesystem>
#include <atomic>
#include <cstdio>
#include <csignal>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "../include/frostmonitor/config.hpp"
#include "../include/frostmonitor/version.hpp"

namespace {
    std::atomic_bool gRunning{true};

    void handleSignal(int){
        gRunning.store(false);
    }

    void setupLogging(const frostmonitor::Config &config){
        std::error_code ec;
        std::filesystem::create_directories(config.logging.dir, ec);

        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            (config.logging.dir / "frostmonitor.log").string(),
            config.logging.maxBytes,
            config.logging.maxFiles
        );

        auto logger = std::make_shared<spdlog::logger>("frostmonitor", spdlog::sinks_init_list{consoleSink, fileSink});
        const auto &level = config.logging.level;

        if(level == "debug") 
            logger->set_level(spdlog::level::debug);
        else if(level == "warn")
            logger->set_level(spdlog::level::warn);
        else if(level == "error")
            logger->set_level(spdlog::level::err);
        else
            logger->set_level(spdlog::level::info);

        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        spdlog::set_default_logger(logger);
    }
}

int main(int argc, char *argv[]){
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    const std::filesystem::path configPath = 
        argc > 1 ? std::filesystem::path{argv[1]}
                 : std::filesystem::path{"config/config.json"}; 

    auto config = frostmonitor::loadConfig(configPath);

    if(!config){
        std::fprintf(stderr, "Failed to load config '%s'\n", configPath.string().c_str());
        exit(EXIT_FAILURE);
    }

    setupLogging(*config);

    spdlog::info("{} v{} starting", frostmonitor::appName, frostmonitor::appVersion);
    spdlog::info("config: {}", configPath.string());
    spdlog::debug("polling interval: {} ms", config->pollingInterval.count());

    unsigned long cycle = 0l;

    while(gRunning.load()){
        // Placeholder stuff for now:
        spdlog::info("cycle {}: telemetry pipeline not wired yet", cycle++);
        std::this_thread::sleep_for(config->pollingInterval);
    }

    spdlog::info("shutdown signal received, exiting cleanly");
    spdlog::shutdown();

    return 0;
}