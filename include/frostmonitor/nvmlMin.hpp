#pragma once

extern "C" {
    using nvmlReturn_t = unsigned int;
    using nvmlDevice_t = void*;

    struct nvmlUtilization_t {
        unsigned int gpu;
        unsigned int memory;
    };
}

inline constexpr int NVML_TEMP_GPU = 0;