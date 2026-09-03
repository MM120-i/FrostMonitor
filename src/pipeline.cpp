#include "../include/frostmonitor/pipeline.hpp"
#include "../include/frostmonitor/format.hpp"
#include "../include/frostmonitor/version.hpp"
#include "../include/frostmonitor/fps.hpp"

#include <spdlog/spdlog.h>

#include <type_traits>

namespace frostmonitor {
    Pipeline::Pipeline(Config config, bool demoMode)
        : config_(std::move(config)), demoMode_(demoMode), fps_(std::nullopt) {
    }

    Pipeline::~Pipeline(){
        {
            std::scoped_lock lock(workerMutex_);

            if(worker_.joinable())
                worker_.request_stop();
        }

        cv_.notify_all();

        if(worker_.joinable())
            worker_.join();
    }

    auto Pipeline::run() -> int {
        if(!demoMode_){
            auto cpuCreated = CpuMonitor::create();

            if(!cpuCreated){
                spdlog::error("CPU Sensor: {}", toString(cpuCreated.error()));
                return static_cast<int>(ExitCode::SENSOR_ERROR);
            }

            cpu_ = std::move(*cpuCreated);
            
            auto gpuCreated = GpuMonitor::create();

            if(!gpuCreated){
                spdlog::error("GPU Sensor: {}", toString(gpuCreated.error()));
                return static_cast<int>(ExitCode::SENSOR_ERROR);
            }

            gpu_ = std::move(*gpuCreated);

            auto fpsCreated = FpsMonitor::create();

            if(fpsCreated)
                fps_ = std::move(*fpsCreated);
            else
                spdlog::info("FPS Sensor: {} (RTSS not running?)", toString(fpsCreated.error()));
        }

        client_ = createGameSenseClient(config_);

        if(client_)
            spdlog::info("Gamesense client ready for game '{}'", client_->game());
        else
            spdlog::info("GameSense disabled");

        {
            std::scoped_lock lock(workerMutex_);

            worker_ = std::jthread([this](const std::stop_token &st){
                workerLoop(st);
            });
        }

        if(stopRequested_.exchange(false, std::memory_order_relaxed)){
            std::scoped_lock lock(workerMutex_);
            worker_.request_stop();
            cv_.notify_all();
        }

        worker_.join();
        return EXIT_SUCCESS;
    }

    void Pipeline::requestStop(){
        stopRequested_.store(true, std::memory_order_relaxed);

        {
            std::scoped_lock lock(workerMutex_);
            worker_.request_stop();
        }

        cv_.notify_all();
    }

    void Pipeline::pause(){
        paused_.store(true, std::memory_order_relaxed);
        cv_.notify_all();
    }

    void Pipeline::resume(){
        paused_.store(false, std::memory_order_relaxed);
        cv_.notify_all();
    }

    auto Pipeline::isPaused() const noexcept -> bool {
        return paused_.load(std::memory_order_relaxed);
    }

    auto Pipeline::stats() const noexcept -> PipelineStats {
        return {
            .totalCycles=totalCycles_.load(std::memory_order_relaxed),
            .droppedCpuReads=droppedCpuReads_.load(std::memory_order_relaxed),
            .droppedGpuReads=droppedGpuReads_.load(std::memory_order_relaxed),
            .droppedFpsReads=droppedFpsReads_.load(std::memory_order_relaxed)
        };
    }

    void Pipeline::interruptibleSleep() {
        std::unique_lock lock(cvMutex_);
        
        cv_.wait_for(lock, config_.pollingInterval, [this] {
            return paused_.load(std::memory_order_relaxed);
        });
    }

    void Pipeline::workerLoop(const std::stop_token &stopToken){ // NOLINT(readability-function-cognitive-complexity)
        while(!stopToken.stop_requested()){
            if(paused_.load(std::memory_order_relaxed)){
                std::unique_lock lock(cvMutex_);

                cv_.wait_for(lock, stopToken, config_.pollingInterval, [] {
                    return false;
                });

                continue;
            }

            std::string cpuLine;
            std::string gpuLine;

            if(demoMode_){
                cpuLine = "CPU 62C | 34%";
                gpuLine = "GPU 55C | 12%";
            }
            else{
                if(!cpu_.has_value() || !gpu_.has_value()){
                    spdlog::error("Sensors not initialized");
                    break;
                }

                auto cpuSample = cpu_->read();

                if(!cpuSample){
                    spdlog::warn("CPU Read failed: {}", toString(cpuSample.error()));
                    droppedCpuReads_.fetch_add(1, std::memory_order_relaxed);
                    interruptibleSleep();
                    continue;
                }

                auto gpuSample = gpu_->read();

                if(!gpuSample){
                    spdlog::warn("GPU Read failed: {}", toString(gpuSample.error()));
                    droppedGpuReads_.fetch_add(1, std::memory_order_relaxed);
                    interruptibleSleep();
                    continue;
                }

                cpuLine = formatCpuLine(cpuSample->tempC, cpuSample->utilizationPct);
                gpuLine = formatGpuLine(gpuSample->tempC, gpuSample->utilizationPct);
            }

            std::string fpsLine;

            if(fps_){
                auto fpsSample = fps_->read();

                if(fpsSample)
                    fpsLine = formatFpsLine(fpsSample->fps);
                else
                    droppedFpsReads_.fetch_add(1, std::memory_order_relaxed);
            }

            spdlog::debug("cycle {}: {} | {}", totalCycles_.load(std::memory_order_relaxed), cpuLine, gpuLine);

            if(client_){
                client_->send(config_.cpuEvent.name, cpuLine);
                client_->send(config_.gpuEvent.name, gpuLine);

                if(!fpsLine.empty())
                    client_->send(config_.fpsEvent.name, fpsLine);
            }

            const auto cycle = totalCycles_.fetch_add(1, std::memory_order_relaxed) + 1;

            if(cycle % 60 == 0)
                spdlog::info("stats: cycles={} dropped_cpu={} dropped_gpu={} dropped_fps={}", cycle, droppedCpuReads_.load(std::memory_order_relaxed), droppedGpuReads_.load(std::memory_order_relaxed), droppedFpsReads_.load(std::memory_order_relaxed));

            interruptibleSleep();
        }

        spdlog::debug("final stats: cycles={} dropped_cpu={} dropped_gpu={} dropped_fps={}", totalCycles_.load(std::memory_order_relaxed), droppedCpuReads_.load(std::memory_order_relaxed), droppedGpuReads_.load(std::memory_order_relaxed), droppedFpsReads_.load(std::memory_order_relaxed));
    }
}