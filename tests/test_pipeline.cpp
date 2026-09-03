#include <chrono>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "../include/frostmonitor/pipeline.hpp"

namespace {
    auto demoConfig() -> frostmonitor::Config {
        frostmonitor::Config config;
        config.pollingInterval = std::chrono::milliseconds{10};
        config.gamesense.registerGame = false;
        return config;
    }
}

TEST_CASE("pipeline demo runs and stops cleanly") {
    auto config = demoConfig();
    frostmonitor::Pipeline pipeline(config, true);

    std::jthread stopper([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
        pipeline.requestStop();
    });

    pipeline.run();

    auto s = pipeline.stats();
    
    CHECK(s.totalCycles > 0);
    CHECK(s.droppedCpuReads == 0);
    CHECK(s.droppedGpuReads == 0);
}

TEST_CASE("pipeline pause prevents cycles") {
    auto config = demoConfig();
    frostmonitor::Pipeline pipeline(config, true);

    pipeline.pause();

    std::jthread stopper([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
        pipeline.requestStop();
    });

    pipeline.run();

    CHECK(pipeline.stats().totalCycles == 0);
}

TEST_CASE("pipeline resume continues cycles") {
    auto config = demoConfig();
    frostmonitor::Pipeline pipeline(config, true);

    pipeline.pause();

    std::jthread resumer([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        pipeline.resume();
        std::this_thread::sleep_for(std::chrono::milliseconds{150});
        pipeline.requestStop();
    });

    pipeline.run();

    CHECK(pipeline.stats().totalCycles > 0);
}

TEST_CASE("pipeline requestStop before run exits fast") {
    auto config = demoConfig();
    frostmonitor::Pipeline pipeline(config, true);

    pipeline.requestStop();

    const auto start = std::chrono::steady_clock::now();
    pipeline.run();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    CHECK(elapsed < std::chrono::milliseconds{100});
    CHECK(pipeline.stats().totalCycles == 0);
}