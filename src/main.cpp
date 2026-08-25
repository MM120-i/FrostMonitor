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
#include "../include/frostmonitor/gamesense.hpp"
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

    auto runPipeline(const frostmonitor::Config &config, bool demoMode) -> int {
        std::optional<frostmonitor::CpuMonitor> cpu;
        std::optional<frostmonitor::GpuMonitor> gpu;

        if(!demoMode){
            auto created = frostmonitor::CpuMonitor::create();

            if(!created){
                spdlog::error("CPU Sensor: {}", frostmonitor::toString(created.error()));
                return 2;   // whats 2??
            }

            cpu = std::move(*created);
            auto gpuCreated = frostmonitor::GpuMonitor::create();

            if(!gpuCreated){
                spdlog::error("GPU Sensor: {}", frostmonitor::toString(gpuCreated.error()));
                return 2;
            }

            gpu = std::move(*gpuCreated);
        }

        auto client = frostmonitor::createGameSenseClient(config);

        if(client)
            spdlog::info("GameSense client ready for game '{}'", client->game());
        else
            spdlog::info("GameSense disabled (register_game = false)");

        unsigned long cycle = 0L;

        while(gRunning.load()){
            std::string cpuLine;
            std::string gpuLine;

            if(demoMode){
                cpuLine = "CPU 62°C | 34%";
                gpuLine = "GPU 55°C | 12%";
            }
            else {
                auto cpuSample = cpu->read();

                if(!cpuSample){
                    spdlog::warn("CPU Read failed: {}", frostmonitor::toString(cpuSample.error()));
                    std::this_thread::sleep_for(config.pollingInterval);
                    continue;
                }

                auto gpuSample = gpu->read();

                if(!gpuSample){
                    spdlog::warn("GPU Read failed: {}", frostmonitor::toString(gpuSample.error()));
                    std::this_thread::sleep_for(config.pollingInterval);
                    continue;
                }

                cpuLine = frostmonitor::formatCpuLine(cpuSample->tempC, cpuSample->utilizationPct);
                gpuLine = frostmonitor::formatGpuLine(gpuSample->tempC, gpuSample->utilizationPct);
            }

            spdlog::debug("cycle {}: {} | {}", cycle++, cpuLine, gpuLine);

            if(client){
                client->send(config.cpuEvent.name, cpuLine);
                client->send(config.gpuEvent.name, gpuLine);
            }

            std::this_thread::sleep_for(config.pollingInterval);
        }

        return EXIT_SUCCESS;
    }
}

auto run(int argc, char **argv) -> int {
    if (argc > 1 && std::string_view(argv[1]) == "--check-sensors")
        return runCheckSensors();

    bool demoMode = false;
    int argIndex = 1;

    if(argc > 1 && std::string_view(argv[1]) == "--demo"){
        demoMode = true;
        argIndex++;
    }

    const std::filesystem::path configPath =
        argc > argIndex ? std::filesystem::path{argv[argIndex]}
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

    return runPipeline(*config, demoMode);
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