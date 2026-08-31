#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

#include "config.hpp"
#include "gamesense.hpp"
#include "cpu.hpp"
#include "gpu.hpp"

namespace frostmonitor {
    struct PipelineStats {
        unsigned long totalCycles{0};
        unsigned long droppedCpuReads{0};
        unsigned long droppedGpuReads{0};
    };

    class Pipeline {
    private:
        void workerLoop(const std::stop_token &stopToken);
        void interruptibleSleep();

        Config config_;
        bool demoMode_;
        PipelineStats stats_{};
        
        std::atomic_bool paused_{false};
        std::jthread worker_;
        std::condition_variable_any cv_;
        std::mutex cvMutex_;
        std::unique_ptr<GameSenseClient> client_;
        std::optional<CpuMonitor> cpu_;
        std::optional<GpuMonitor> gpu_;

    public:
        Pipeline(Config config, bool demoMode = false);
        ~Pipeline();

        Pipeline(const Pipeline &) = delete;
        Pipeline(Pipeline &&) = delete;
        Pipeline &operator = (const Pipeline &) = delete;
        Pipeline &operator = (Pipeline &&) = delete;

        auto run() -> int;
        void requestStop();
        void pause();
        void resume();

        [[nodiscard]] auto isPaused() const noexcept -> bool;
        [[nodiscard]] auto stats() const noexcept -> PipelineStats;
    };
}