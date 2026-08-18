cmake_minimum_required(VERSION 3.27)

project(FrostMonitor
    VERSION 0.1.0
    DESCRIPTION "CPU/GPU telemetry to SteelSeries GameSense OLED"
    LANGUAGES CXX
)
    
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

set(CORE_SOURCES src/config.cpp src/cpu.cpp src/gpu.cpp src/format.cpp)

add_library(FrostMonitorCore STATIC ${CORE_SOURCES})

target_include_directories(FrostMonitorCore PUBLIC include)

target_compile_definitions(FrostMonitorCore PUBLIC
    FROSTMON_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(FrostMonitorCore PRIVATE /W4 /WX /permissive- /utf-8 $<$<CONFIG:Debug>:/fsanitize=address>)
endif()

target_link_libraries(FrostMonitorCore PUBLIC nlohmann_json::nlohmann_json)

add_library(FrostMonitorCoreClean STATIC ${CORE_SOURCES})

target_include_directories(FrostMonitorCoreClean PUBLIC include)

target_compile_definitions(FrostMonitorCoreClean PUBLIC
    FROSTMON_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(FrostMonitorCoreClean PRIVATE /W4 /WX /permissive- /utf-8)
endif()

target_link_libraries(FrostMonitorCoreClean PUBLIC nlohmann_json::nlohmann_json)

if(MSVC)
    target_link_libraries(FrostMonitorCore PUBLIC pdh.lib)
    target_link_libraries(FrostMonitorCoreClean PUBLIC pdh.lib)
endif()

add_executable(FrostMonitor
    src/main.cpp
)

if(MSVC)
    target_compile_options(FrostMonitor PRIVATE /W4 /WX /permissive- /utf-8 $<$<CONFIG:Debug>:/fsanitize=address>)
    target_link_options(FrostMonitor PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL:NO>)

    get_filename_component(MSVC_TOOLS_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    add_custom_command(TARGET FrostMonitor POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DASAN_DLL="${MSVC_TOOLS_BIN}/clang_rt.asan_dynamic-x86_64.dll"
            -DDEST="$<TARGET_FILE_DIR:FrostMonitor>"
            -DCONFIG=$<CONFIG>
            -P "${CMAKE_SOURCE_DIR}/cmake/copy_asan_dll.cmake"
    )
endif()

target_link_libraries(FrostMonitor PRIVATE
    FrostMonitorCore
    spdlog::spdlog
    httplib::httplib
)

enable_testing()
add_subdirectory(tests)