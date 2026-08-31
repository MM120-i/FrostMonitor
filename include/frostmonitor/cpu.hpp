#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <windows.h>
#include <Pdh.h>

namespace frostmonitor {
    struct CpuSample {
        std::optional<double> tempC;
        int utilizationPct;
    };

    enum class CpuError : std::uint8_t {
        QueryCreateFailed,
        CounterOpenFailed,
        ThermalZoneNotFound,
        CollectFailed,
        ReadFailed,
    };

auto toString(CpuError error) -> const char*;

    class CpuMonitor {
    private:
        struct QueryDeleter {
            void operator()(void *q) const noexcept {
                if(q) 
                    PdhCloseQuery(static_cast<PDH_HQUERY>(q));
            }
        };

        using QueryHandle = std::unique_ptr<void, QueryDeleter>;
        
        CpuMonitor(QueryHandle query, PDH_HCOUNTER utilCounter, PDH_HCOUNTER tempCounter);

        QueryHandle query_;
        PDH_HCOUNTER utilCounter_ = nullptr;
        PDH_HCOUNTER tempCounter_ = nullptr;


    public:
        static auto create() -> std::expected<CpuMonitor, CpuError>;

        CpuMonitor(const CpuMonitor &) = delete;
        CpuMonitor &operator = (const CpuMonitor &) = delete;  
        CpuMonitor(CpuMonitor &&) noexcept = default;
        CpuMonitor &operator = (CpuMonitor &&) noexcept = default; 
        ~CpuMonitor() = default;

        auto read() -> std::expected<CpuSample, CpuError>;
    };

    auto findThermalZoneInstance() -> std::expected<std::wstring, CpuError>;
}