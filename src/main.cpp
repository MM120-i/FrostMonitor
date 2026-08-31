#include <memory>
#include <filesystem>
#include <print>
#include <string_view>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "../include/frostmonitor/config.hpp"
#include "../include/frostmonitor/version.hpp"
#include "../include/frostmonitor/format.hpp"
#include "../include/frostmonitor/pipeline.hpp"

namespace {
    frostmonitor::Pipeline *gPipeline = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    BOOL WINAPI ctrlHandler(DWORD ctrlType) {
        switch(ctrlType) {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
            case CTRL_CLOSE_EVENT:
                spdlog::info("shutdown signal received");

                if(gPipeline != nullptr)
                    gPipeline->requestStop();

                return TRUE;

            default:
                return FALSE;
        }
    }

    void setupLogging(const frostmonitor::Config &config) {
        std::error_code ec;
        std::filesystem::create_directories(config.logging.dir, ec);

        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            (config.logging.dir / "frostmonitor.log").string(),
            config.logging.maxBytes,
            config.logging.maxFiles
        );

        auto logger = std::make_shared<spdlog::logger>(
            "frostmonitor", spdlog::sinks_init_list{consoleSink, fileSink});

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

        if(!cpu) {
            std::println(stderr, "CPU sensor: {}", frostmonitor::toString(cpu.error()));
            return 2;
        }

        auto gpu = frostmonitor::GpuMonitor::create();

        if(!gpu) {
            std::println(stderr, "GPU sensor: {}", frostmonitor::toString(gpu.error()));
            return 2;
        }

        auto cpuSample = cpu->read();

        if(!cpuSample) {
            std::println(stderr, "CPU read: {}", frostmonitor::toString(cpuSample.error()));
            return 2;
        }

        auto gpuSample = gpu->read();

        if(!gpuSample) {
            std::println(stderr, "GPU read: {}", frostmonitor::toString(gpuSample.error()));
            return 2;
        }

        std::println("{}", frostmonitor::formatCpuLine(cpuSample->tempC, cpuSample->utilizationPct));
        std::println("{}", frostmonitor::formatGpuLine(gpuSample->tempC, gpuSample->utilizationPct));
        
        return 0;
    }

    auto run(int argc, char **argv) -> int {
        if(argc > 1 && std::string_view(argv[1]) == "--check-sensors")
            return runCheckSensors();

        bool demoMode = false;
        int argIndex = 1;

        if(argc > 1 && std::string_view(argv[1]) == "--demo") {
            demoMode = true;
            argIndex++;
        }

        const std::filesystem::path configPath =
            argc > argIndex ? std::filesystem::path{argv[argIndex]}
                            : std::filesystem::path{"config/config.json"};

        auto config = frostmonitor::loadConfig(configPath);
        
        if(!config) {
            std::println(stderr, "Failed to load config '{}'", configPath.string());
            return EXIT_FAILURE;
        }

        setupLogging(*config);

        spdlog::info("{} v{} starting", frostmonitor::appName, frostmonitor::appVersion);
        spdlog::info("config: {}", configPath.string());
        spdlog::debug("polling interval: {} ms", config->pollingInterval.count());

        frostmonitor::Pipeline pipeline(std::move(*config), demoMode);
        gPipeline = &pipeline;

        return pipeline.run();
    }
}

auto main(int argc, char **argv) -> int { // NOLINT(bugprone-exception-escape)
    SetConsoleCtrlHandler(ctrlHandler, TRUE);

    try {
        return run(argc, argv);
    }
    catch(const std::exception &e) {
        std::println(stderr, "Fatal error: {}", e.what());
        spdlog::shutdown();
        return EXIT_FAILURE;
    }
}