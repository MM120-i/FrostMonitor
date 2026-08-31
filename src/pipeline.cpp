#include "../include/frostmonitor/pipeline.hpp"
#include "../include/frostmonitor/format.hpp"
#include "../include/frostmonitor/version.hpp"

#include <spdlog/spdlog.h>

#include <type_traits>

namespace frostmonitor {
    Pipeline::Pipeline(Config config, bool demoMode)
        : config_(std::move(config)), demoMode_(demoMode) {
    }

    Pipeline::~Pipeline(){
        if(worker_.joinable()){
            worker_.request_stop();
            cv_.notify_all();
            worker_.join();
        }
    }

    auto Pipeline::run() -> int {
        if(!demoMode_){
            auto cpuCreated = CpuMonitor::create();

            if(!cpuCreated){
                spdlog::error("CPU Sensor: {}", toString(cpuCreated.error()));
                return 2;   // TODO: Change this 2 to an enum value
            }

            cpu_ = std::move(*cpuCreated);
            
            auto gpuCreated = GpuMonitor::create();

            if(!gpuCreated){
                spdlog::error("GPU Sensor: {}", toString(gpuCreated.error()));
                return 2;
            }

            gpu_ = std::move(*gpuCreated);
        }

        client_ = createGameSenseClient(config_);

        if(client_)
            spdlog::info("Gamesense client ready for game '{}'", client_->game());
        else
            spdlog::info("GameSense disabled");

        worker_ = std::jthread([this](const std::stop_token &st){
            workerLoop(st);
        });

        worker_.join();
        return EXIT_SUCCESS;
    }

    void Pipeline::requestStop(){
        worker_.request_stop();
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
        return stats_;
    }

    void Pipeline::interruptibleSleep() {
        std::unique_lock lock(cvMutex_);
        
        cv_.wait_for(lock, config_.pollingInterval, [this] {
            return paused_.load(std::memory_order_relaxed);
        });
    }

    void Pipeline::workerLoop(const std::stop_token &stopToken){
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
                    stats_.droppedCpuReads++;
                    interruptibleSleep();
                    continue;
                }

                auto gpuSample = gpu_->read();

                if(!gpuSample){
                    spdlog::warn("GPU Read failed: {}", toString(gpuSample.error()));
                    stats_.droppedGpuReads++;
                    interruptibleSleep();
                    continue;
                }

                cpuLine = formatCpuLine(cpuSample->tempC, cpuSample->utilizationPct);
                gpuLine = formatGpuLine(gpuSample->tempC, gpuSample->utilizationPct);
            }

            spdlog::debug("cycle {}: {} | {}", stats_.totalCycles, cpuLine, gpuLine);

            if(client_){
                client_->send(config_.cpuEvent.name, cpuLine);
                client_->send(config_.gpuEvent.name, gpuLine);
            }

            stats_.totalCycles++;

            if(stats_.totalCycles % 60 == 0) 
                spdlog::info("stats: cycles={} dropped_cpu={} dropped_gpu={}", stats_.totalCycles, stats_.droppedCpuReads, stats_.droppedGpuReads);

            interruptibleSleep();    
        }

        spdlog::debug("final stats: cycles={} dropped_cpu={} dropped_gpu={}", stats_.totalCycles, stats_.droppedCpuReads, stats_.droppedGpuReads);
    }
}