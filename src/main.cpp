#include <atomic>
#include <memory>
#include <filesystem>
#include <print>
#include <string_view>
#include <thread>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "../include/frostmonitor/autostart.hpp"
#include "../include/frostmonitor/config.hpp"
#include "../include/frostmonitor/version.hpp"
#include "../include/frostmonitor/format.hpp"
#include "../include/frostmonitor/pipeline.hpp"
#include "../include/frostmonitor/single_instance.hpp"
#include "../include/frostmonitor/tray.hpp"

#include <windows.h>
#include <shellapi.h>

namespace {
    frostmonitor::Pipeline *gPipeline = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    using path = std::filesystem::path;

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

    int runCheckSensors(){
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

    struct CliOptions {
        bool demoMode{false};
        bool trayMode{false};
        int argIndex{1};
    };

    auto parseCliOptions(int argc, char **argv) -> CliOptions {
        CliOptions options;

        for(; options.argIndex < argc; options.argIndex++){
            const std::string_view arg{argv[options.argIndex]};

            if(arg == "--demo")
                options.demoMode = true;
            else if(arg == "--tray")
                options.trayMode = true;
            else
                break;
        }

        return options;
    }

    int run(int argc, char **argv){
        if(argc > 1 && std::string_view(argv[1]) == "--check-sensors")
            return runCheckSensors();

        const auto options = parseCliOptions(argc, argv);
        const bool demoMode = options.demoMode;
        const bool trayMode = options.trayMode;
        const int argIndex = options.argIndex;

        if(trayMode){
            HWND console = GetConsoleWindow();

            if(console != nullptr)
                ShowWindow(console, SW_HIDE);
        }

        frostmonitor::SingleInstanceGuard instance(frostmonitor::kSingleInstanceMutexName);

        if(!instance.isPrimary()){
            std::println(stderr, "FrostMonitor is already running");
            return static_cast<int>(frostmonitor::ExitCode::ALREADY_RUNNING);
        }

        const path configPath =
            argIndex < argc ? path{argv[argIndex]}
                            : path{"config/config.json"};

        auto config = frostmonitor::loadConfig(configPath);

        if(!config){
            std::println(stderr, "Failed to load config '{}'", configPath.string());
            return EXIT_FAILURE;
        }

        setupLogging(*config);

        spdlog::info("{} v{} starting", frostmonitor::appName, frostmonitor::appVersion);

        if(!demoMode){
            const path exePath = frostmonitor::currentExePath();
            const path absoluteConfig = std::filesystem::absolute(configPath);

            if(!frostmonitor::syncAutoStart(config->autoStart, exePath, absoluteConfig))
                spdlog::warn("continuing without autostart sync");
        }

        frostmonitor::Pipeline pipeline(std::move(*config), demoMode);
        gPipeline = &pipeline;

        const std::wstring openTarget = std::filesystem::absolute(configPath).wstring();
        const std::string openTargetLog = std::filesystem::absolute(configPath).string();

        frostmonitor::TrayIcon tray(GetModuleHandleW(nullptr), {
            .onTogglePause = [&]{
                if(pipeline.isPaused()){
                    pipeline.resume();
                    tray.setPaused(false);
                }
                else {
                    pipeline.pause();
                    tray.setPaused(true);
                }
            },
            .onOpenConfig = [&]{
                const HINSTANCE result = ShellExecuteW(nullptr, L"open", openTarget.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

                if(reinterpret_cast<INT_PTR>(result) <= 32)
                    spdlog::warn("tray: cannot open config '{}'", openTargetLog);
            },
            .onExit = [&]{ pipeline.requestStop(); },
        });

        std::atomic<int> exitCode{EXIT_SUCCESS};
        
        std::jthread pipelineThread([&]{
            exitCode.store(pipeline.run(), std::memory_order_relaxed);
            PostMessageW(tray.window(), WM_CLOSE, 0, 0);
        });

        MSG msg{};
        BOOL hasMessage = GetMessageW(&msg, nullptr, 0, 0);

        while(hasMessage > 0){
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            hasMessage = GetMessageW(&msg, nullptr, 0, 0);
        }

        pipelineThread.join();
        gPipeline = nullptr;
        return exitCode.load(std::memory_order_relaxed);
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