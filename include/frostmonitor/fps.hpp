#pragma once

#include <cstdint>
#include <expected>
#include <string>

namespace frostmonitor {
    constexpr std::uint32_t kRtssSignature = 0x53535452; // NOLINT(cppcoreguidelines-avoid-magic-numbers)
    constexpr std::uint32_t kRtssMinVersion = 0x00020000; 
    constexpr int kMaxPath = 260; // NOLINT(cppcoreguidelines-avoid-magic-numbers)

    #pragma pack(push, 1)
    struct RtssAppEntry {
        std::uint32_t processId;
        char name[kMaxPath]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
        std::uint32_t flags;
        std::uint32_t time0;
        std::uint32_t time1;
        std::uint32_t frames;
        std::uint32_t frameTime;
    };

    struct RtssHeader {
        std::uint32_t signature;
        std::uint32_t version;
        std::uint32_t appEntrySize;
        std::uint32_t appArrOffset;
        std::uint32_t appArrSize;
    };

    #pragma pack(pop)
    enum class FpsError : std::uint8_t {
        SharedMemoryOpenFailed,
        MapViewFailed,
        InvalidSignature,
        UnsupportedVersion,
        InvalidLayout,
        NoForegroundProcess,
        ProcessNotFound,
    };

    auto toString(FpsError error) -> const char *;

    struct FpsSample {
        int fps;
        std::uint32_t processId;
    };

    class FpsMonitor {
    private:
        FpsMonitor(void *mapping, std::uint32_t appArrOffset, std::uint32_t appEntrySize, std::uint32_t appArrSize);

        void *mapping_;
        std::uint32_t appArrOffset_;
        std::uint32_t appEntrySize_;
        std::uint32_t appArrSize_;

    public:
        static auto create() -> std::expected<FpsMonitor, FpsError>;

        ~FpsMonitor();
        FpsMonitor(const FpsMonitor &) = delete;
        FpsMonitor(FpsMonitor &&other) noexcept;
        FpsMonitor &operator = (const FpsMonitor &) = delete;
        FpsMonitor &operator = (FpsMonitor &&other) noexcept;

        auto read() -> std::expected<FpsSample, FpsError>;
    };
}   