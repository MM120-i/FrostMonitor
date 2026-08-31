#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <windows.h>

#include "nvmlMin.hpp"

namespace frostmonitor {
    struct GpuSample {
        double tempC;
        int utilizationPct;
    };

    enum class GpuError : std::uint8_t {
        DllLoadFailed,
        FunctionLookupFailed,
        InitFailed,
        DeviceLookupFailed,
        ReadFailed,
    };

    auto toString(GpuError e) -> const char *;

    class GpuMonitor {
    private:
        using InitFn = nvmlReturn_t(__cdecl*)(void);
        using ShutdownFn = nvmlReturn_t(__cdecl*)(void);
        using GetDeviceFn = nvmlReturn_t(__cdecl*)(unsigned int, nvmlDevice_t*);
        using GetTemperatureFn = nvmlReturn_t(__cdecl*)(nvmlDevice_t, int, unsigned int*);
        using GetUtilizationFn = nvmlReturn_t(__cdecl*)(nvmlDevice_t, nvmlUtilization_t*);
        using ErrorStringFn = const char *(__cdecl*)(nvmlReturn_t);

        GpuMonitor(HMODULE lib, nvmlDevice_t device, InitFn init, ShutdownFn shutdown, GetTemperatureFn temperature, GetUtilizationFn utilization);
        
        HMODULE lib_ = nullptr;
        nvmlDevice_t device_ = nullptr;
        InitFn init_;
        ShutdownFn shutdown_;
        GetTemperatureFn temperature_;
        GetUtilizationFn utilization_;

    public:
        static auto create() -> std::expected<GpuMonitor, GpuError>;

        GpuMonitor(const GpuMonitor &) = delete;
        GpuMonitor &operator = (const GpuMonitor &) = delete;  
        GpuMonitor &operator = (GpuMonitor &&other) noexcept;  
        ~GpuMonitor();

        auto read() -> std::expected<GpuSample, GpuError>;
    };
}