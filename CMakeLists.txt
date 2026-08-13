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

set(CORE_SOURCES src/config.cpp)

# Sanitized core: linked into the app (ASAN instrumented)
add_library(FrostMonitorCore STATIC ${CORE_SOURCES})

target_include_directories(FrostMonitorCore PUBLIC include)

target_compile_definitions(FrostMonitorCore PUBLIC
    FROSTMON_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(FrostMonitorCore PRIVATE /W4 /WX /permissive- /utf-8 $<$<CONFIG:Debug>:/fsanitize=address>)
endif()

target_link_libraries(FrostMonitorCore PUBLIC nlohmann_json::nlohmann_json)

# Clean core: same sources, no ASAN - linked into tests (Catch2 is prebuilt without ASAN)
add_library(FrostMonitorCoreClean STATIC ${CORE_SOURCES})

target_include_directories(FrostMonitorCoreClean PUBLIC include)

target_compile_definitions(FrostMonitorCoreClean PUBLIC
    FROSTMON_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(FrostMonitorCoreClean PRIVATE /W4 /WX /permissive- /utf-8)
endif()

target_link_libraries(FrostMonitorCoreClean PUBLIC nlohmann_json::nlohmann_json)

add_executable(FrostMonitor
    src/main.cpp
)

if(MSVC)
    target_compile_options(FrostMonitor PRIVATE /W4 /WX /permissive- /utf-8 $<$<CONFIG:Debug>:/fsanitize=address>)
    target_link_options(FrostMonitor PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL:NO>)
endif()

target_link_libraries(FrostMonitor PRIVATE
    FrostMonitorCore
    spdlog::spdlog
    httplib::httplib
)

enable_testing()
add_subdirectory(tests)