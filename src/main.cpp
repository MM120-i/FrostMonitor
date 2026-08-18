#include <thread>
#include <memory>
#include <filesystem>
#include <atomic>
#include <csignal>
#include <exception>
#include <print>
#include <string_view> 

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "../include/frostmonitor/config.hpp"
#include "../include/frostmonitor/version.hpp"                        
#include "../include/frostmonitor/cpu.hpp"      
#include "../include/frostmonitor/gpu.hpp"      
#include "../include/frostmonitor/format.hpp"   

namespace {
    std::atomic_bool gRunning{true}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    void handleSignal(int signum){
        (void)signum;
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

    auto runCheckSensors() -> int {
        auto cpu = frostmonitor::CpuMonitor::create();

        if (!cpu) {
            std::println(stderr, "CPU sensor: {}", frostmonitor::toString(cpu.error()));
            return 2;
        }

        auto gpu = frostmonitor::GpuMonitor::create();

        if (!gpu) {
            std::println(stderr, "GPU sensor: {}", frostmonitor::toString(gpu.error()));
            return 2;
        }

        auto cpuSample = cpu->read();

        if (!cpuSample) {
            std::println(stderr, "CPU read: {}", frostmonitor::toString(cpuSample.error()));
            return 2;
        }

        auto gpuSample = gpu->read();

        if (!gpuSample) {
            std::println(stderr, "GPU read: {}", frostmonitor::toString(gpuSample.error()));
            return 2;
        }

        std::println("{}", frostmonitor::formatCpuLine(cpuSample->tempC, cpuSample->utilizationPct));
        std::println("{}", frostmonitor::formatGpuLine(gpuSample->tempC, gpuSample->utilizationPct));
        
        return 0;
    }
}

auto run(int argc, char **argv) -> int {
    if (argc > 1 && std::string_view(argv[1]) == "--check-sensors")
        return runCheckSensors();

    const std::filesystem::path configPath = 
        argc > 1 ? std::filesystem::path{argv[1]}
                 : std::filesystem::path{"config/config.json"}; 

    auto config = frostmonitor::loadConfig(configPath);

    if(!config){
        std::println(stderr, "Failed to load config '{}'", configPath.string());
        return EXIT_FAILURE;
    }

    setupLogging(*config);

    spdlog::info("{} v{} starting", frostmonitor::appName, frostmonitor::appVersion);
    spdlog::info("config: {}", configPath.string());
    spdlog::debug("polling interval: {} ms", config->pollingInterval.count());

    unsigned long cycle = 0L;

    while(gRunning.load()){
        // Placeholder stuff for now:
        spdlog::info("cycle {}: telemetry pipeline not wired yet", cycle++);
        std::this_thread::sleep_for(config->pollingInterval);
    }

    spdlog::info("shutdown signal received, exiting cleanly");
    spdlog::shutdown();

    return EXIT_SUCCESS;
}

auto main(int argc, char **argv) -> int { // NOLINT(bugprone-exception-escape)
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    try {
        return run(argc, argv);
    }
    catch(const std::exception &e){
        std::println(stderr, "Fatal error: {}", e.what());
        spdlog::shutdown();
        return EXIT_FAILURE;
    }
}