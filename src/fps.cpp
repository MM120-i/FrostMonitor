#include "../include/frostmonitor/fps.hpp"

#include <spdlog/spdlog.h>

#include <windows.h>

namespace frostmonitor {
    auto toString(FpsError error) -> const char * {
        switch(error){
            case FpsError::SharedMemoryOpenFailed:
                return "RTSS shared memory not found (is RTSS running?)";
            case FpsError::MapViewFailed:
                return "failed to map RTSS shared memory";
            case FpsError::InvalidSignature:
                return "RTSS shared memory has invalid signature";
            case FpsError::UnsupportedVersion:
                return "RTSS shared memory version too old (need v2.0+)";
            case FpsError::NoForegroundProcess:
                return "no foreground window found";
            case FpsError::ProcessNotFound:
                return "foreground process not found in RTSS data";
            default:
                return "unknown FPS error";
        }
    }

    auto FpsMonitor::create() -> std::expected<FpsMonitor, FpsError> {
        HANDLE hMap = OpenFileMappingW(FILE_MAP_READ, FALSE, L"RTSSSharedMemoryV2");

        if(hMap == nullptr)
            return std::unexpected(FpsError::SharedMemoryOpenFailed);

        void *mapping = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
        CloseHandle(hMap);

        if(mapping == nullptr)
            return std::unexpected(FpsError::MapViewFailed);

        const auto *header = static_cast<const RtssHeader *>(mapping);

        if(header->signature != kRtssSignature){
            UnmapViewOfFile(mapping);
            return std::unexpected(FpsError::InvalidSignature);
        }

        if(header->version < kRtssMinVersion){
            UnmapViewOfFile(mapping);
            return std::unexpected(FpsError::UnsupportedVersion);
        }

        return FpsMonitor(mapping, header->appArrOffset, header->appEntrySize, header->appArrSize);
    }

    FpsMonitor::~FpsMonitor(){
        if(mapping_ != nullptr)
            UnmapViewOfFile(mapping_);
    }

    FpsMonitor::FpsMonitor(FpsMonitor &&other) noexcept :
        mapping_(other.mapping_), appArrOffset_(other.appArrOffset_),
        appEntrySize_(other.appEntrySize_), appArrSize_(other.appArrSize_)
    {
        other.mapping_ = nullptr;
    }

    FpsMonitor &FpsMonitor::operator = (FpsMonitor &&other) noexcept {
        if(this != &other){
            if(mapping_ != nullptr)
                UnmapViewOfFile(mapping_);

            mapping_ = other.mapping_;
            appArrOffset_ = other.appArrOffset_;
            appEntrySize_ = other.appEntrySize_;
            appArrSize_ = other.appArrSize_;
            other.mapping_ = nullptr;
        }

        return *this;
    }

    FpsMonitor::FpsMonitor(void *mapping, std::uint32_t appArrOffset, std::uint32_t appEntrySize, std::uint32_t appArrSize): // NOLINT(bugprone-easily-swappable-parameters)
        mapping_(mapping), appArrOffset_(appArrOffset), appEntrySize_(appEntrySize), appArrSize_(appArrSize) {}

    auto FpsMonitor::read() -> std::expected<FpsSample, FpsError> {
        HWND foreground = GetForegroundWindow();

        if(foreground == nullptr)
            return std::unexpected(FpsError::NoForegroundProcess);

        DWORD pid = 0;
        GetWindowThreadProcessId(foreground, &pid);

        if(pid == 0)
            return std::unexpected(FpsError::NoForegroundProcess);

        const auto *base = static_cast<const std::uint8_t *>(mapping_);

        for(std::uint32_t i = 0; i < appArrSize_; ++i){
            const auto *entry = reinterpret_cast<const RtssAppEntry *>(base + appArrOffset_ + (static_cast<size_t>(i) * appEntrySize_));

            if(entry->processId == pid){
                const auto delta = entry->time1 - entry->time0;

                if(delta == 0)
                    return FpsSample{.fps=0, .processId=pid};

                const int fps = static_cast<int>(entry->frames * 1000ULL / delta);

                return FpsSample{.fps=fps, .processId=pid};
            }
        }

        return std::unexpected(FpsError::ProcessNotFound);
    }
}