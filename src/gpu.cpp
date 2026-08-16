#include <utility>
#include <spdlog/spdlog.h>

#include "../include/frostmonitor/gpu.hpp"

namespace frostmonitor {
    auto toString(GpuError e) -> const char* {
        switch (e) {
            case GpuError::DllLoadFailed:
                return "nvml.dll not found (no NVIDIA GPU?)";
            case GpuError::FunctionLookupFailed: 
                return "nvml.dll is missing an expected function";
            case GpuError::InitFailed:         
                return "nvmlInit failed";
            case GpuError::DeviceLookupFailed: 
                return "no NVIDIA device found";
            case GpuError::ReadFailed:         
                return "failed to read GPU telemetry";
            default:
                return "unknown GPU error";
        }
    }

    auto GpuMonitor::create() -> std::expected<GpuMonitor, GpuError> {
        HMODULE lib = LoadLibraryW(L"nvml.dll");

        if(!lib) // NOLINT(readability-implicit-bool-conversion)
            return std::unexpected(GpuError::DllLoadFailed);

        auto init = reinterpret_cast<InitFn>(GetProcAddress(lib, "nvmlInit_v2"));
        auto shutdown = reinterpret_cast<ShutdownFn>(GetProcAddress(lib, "nvmlShutdown"));
        auto getDevice = reinterpret_cast<GetDeviceFn>(GetProcAddress(lib, "nvmlDeviceGetHandleByIndex_v2"));
        auto temperature = reinterpret_cast<GetTemperatureFn>(GetProcAddress(lib, "nvmlDeviceGetTemperature"));
        auto utilization= reinterpret_cast<GetUtilizationFn>(GetProcAddress(lib, "nvmlDeviceGetUtilizationRates"));

        if(!init || !shutdown || !getDevice || !temperature || !utilization){ // NOLINT(readability-implicit-bool-conversion)
            FreeLibrary(lib);
            return std::unexpected(GpuError::FunctionLookupFailed);
        }

        if(init() != 0){ // NOLINT(clang-analyzer-core.CallAndMessage)
            FreeLibrary(lib);
            return std::unexpected(GpuError::InitFailed);
        }

        nvmlDevice_t device = nullptr;

        if(getDevice(0, &device) != 0){
            shutdown();
            FreeLibrary(lib);
            return std::unexpected(GpuError::DeviceLookupFailed);
        }

        return GpuMonitor(lib, device, init, shutdown, temperature, utilization);
    }

    GpuMonitor::GpuMonitor(
        HMODULE lib, 
        nvmlDevice_t device, 
        InitFn init, 
        ShutdownFn shutdown, 
        GetTemperatureFn temperature, 
        GetUtilizationFn utilization
    ): lib_(lib), device_(device), init_(init), shutdown_(shutdown),
       temperature_(temperature), utilization_(utilization) {}

    GpuMonitor::GpuMonitor(GpuMonitor &&others) noexcept
    : lib_(std::exchange(others.lib_, nullptr)),
      device_(std::exchange(others.device_, nullptr)),
      init_(others.init_), shutdown_(others.shutdown_),
      temperature_(others.temperature_), utilization_(others.utilization_) {}

    GpuMonitor &GpuMonitor::operator = (GpuMonitor &&other) noexcept {  // NOLINT(modernize-use-trailing-return-type)
if(this != &other){
            if(lib_ != nullptr){
                shutdown_();
                FreeLibrary(lib_);
            }

            lib_ = std::exchange(other.lib_, nullptr);
            device_ = std::exchange(other.device_, nullptr);
            init_ = other.init_;
            shutdown_ = other.shutdown_;
            temperature_ = other.temperature_;
            utilization_ = other.utilization_;
        }

        return *this;
    }

    GpuMonitor::~GpuMonitor(){
        if(lib_ != nullptr){
            shutdown_();
            FreeLibrary(lib_);
            lib_ = nullptr;
        }
    }

    auto GpuMonitor::read() -> std::expected<GpuSample, GpuError> {
        unsigned int tempC = 0;

        if(temperature_(device_, NVML_TEMP_GPU, &tempC) != 0)
            return std::unexpected(GpuError::ReadFailed);

        nvmlUtilization_t util{};

        if(utilization_(device_, &util) != 0)
            return std::unexpected(GpuError::ReadFailed);

        return GpuSample(
            static_cast<double>(tempC),
            static_cast<int>(util.gpu)
        );
    }
}