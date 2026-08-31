#include <cmath>
#include <windows.h>
#include <pdhmsg.h>

#include "../include/frostmonitor/cpu.hpp"

namespace {
    auto pdhDoubleValue(const PDH_FMT_COUNTERVALUE &value) -> double {
        return value.doubleValue; 
    }
}

namespace frostmonitor {
    auto toString(CpuError e) -> const char* {
        switch (e){
            case CpuError::QueryCreateFailed:
                return "Failed to create PDH query";
            
            case CpuError::CounterOpenFailed:
                return "Failed to open PDH counter";

            case CpuError::ThermalZoneNotFound:
                return "No ACPI thermal zone found";

            case CpuError::CollectFailed:
                return "PDH sample collection failed";

            case CpuError::ReadFailed:
                return "Failed to read counter value";
            
            default:
                return "Unknown CPU error";
        }
    }

    auto findThermalZoneInstance() -> std::expected<std::wstring, CpuError> {
        DWORD counterChars = 0;
        DWORD instanceChars = 0;

        if(PdhEnumObjectItemsW(nullptr, nullptr, L"Thermal Zone Information",
                               nullptr, &counterChars, nullptr, &instanceChars,
                               PERF_DETAIL_WIZARD, 0) != ERROR_SUCCESS || instanceChars == 0)
            return std::unexpected(CpuError::ThermalZoneNotFound);

        std::wstring instances(instanceChars + 1, L'\0');

        if(PdhEnumObjectItemsW(nullptr, nullptr, L"Thermal Zone Information",
                               nullptr, &counterChars, instances.data(), &instanceChars,
                               PERF_DETAIL_WIZARD, 0) != ERROR_SUCCESS)
            return std::unexpected(CpuError::ThermalZoneNotFound);

        if(instances.empty() || instances.front() == L'\0')
            return std::unexpected(CpuError::ThermalZoneNotFound);

        return std::wstring(instances.data()); // NOLINT(readability-redundant-string-cstr)
    }

    auto CpuMonitor::create() -> std::expected<CpuMonitor, CpuError> {
        PDH_HQUERY raw = nullptr;

        if(PdhOpenQuery(nullptr, 0, &raw) != ERROR_SUCCESS)
            return std::unexpected(CpuError::QueryCreateFailed);

        QueryHandle query(raw);
        PDH_HCOUNTER util = nullptr;
        PDH_HCOUNTER temp = nullptr;

        if(PdhAddEnglishCounterW(query.get(), L"\\Processor Information(_Total)\\% Processor Utility", 0, &util) != ERROR_SUCCESS)
            return std::unexpected(CpuError::CounterOpenFailed);

        auto zone = findThermalZoneInstance();

        if(zone){
            std::wstring tempPath = L"\\Thermal Zone Information(" + *zone + L")\\Temperature";

            if(PdhAddEnglishCounterW(query.get(), tempPath.c_str(), 0, &temp) != ERROR_SUCCESS)
                return std::unexpected(CpuError::CounterOpenFailed);
        }

        return CpuMonitor(std::move(query), util, temp);
    }

    CpuMonitor::CpuMonitor(QueryHandle query, PDH_HCOUNTER util, PDH_HCOUNTER temp) : 
        query_(std::move(query)), utilCounter_(util), tempCounter_(temp){}

    auto CpuMonitor::read() -> std::expected<CpuSample, CpuError> {
        if(PdhCollectQueryData(query_.get()) != ERROR_SUCCESS)
            return std::unexpected(CpuError::CollectFailed);

        auto readCounter = [this](PDH_HCOUNTER h) -> std::expected<double, CpuError> {
            PDH_FMT_COUNTERVALUE value{};
            PDH_STATUS status = PdhGetFormattedCounterValue(h, PDH_FMT_DOUBLE, nullptr, &value);

            if(status == PDH_INVALID_DATA){
                ::Sleep(200);

                if(PdhCollectQueryData(query_.get()) != ERROR_SUCCESS)
                    return std::unexpected(CpuError::CollectFailed);

                status = PdhGetFormattedCounterValue(h, PDH_FMT_DOUBLE, nullptr, &value);
            }

            if(status != ERROR_SUCCESS)
                return std::unexpected(CpuError::ReadFailed);

            return pdhDoubleValue(value);
        };

        auto util = readCounter(utilCounter_);

        if(!util)
            return std::unexpected(util.error());

        std::optional<double> celsius;

        if(tempCounter_ != nullptr){
            auto temp = readCounter(tempCounter_);

            if(!temp)
                return std::unexpected(temp.error());

            celsius = (*temp / 10.0) - 273.15;
        }

        const int percent = static_cast<int>(std::lround(*util));
        
        return CpuSample {
            .tempC = celsius,
            .utilizationPct = percent
        };
    }
}