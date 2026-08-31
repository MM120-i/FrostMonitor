find_package(Catch2 CONFIG REQUIRED)

add_executable(FrostMonitorTests
    test_config.cpp
    test_format.cpp
    test_version.cpp
    test_gamesense.cpp
    test_pipeline.cpp
)

target_link_libraries(FrostMonitorTests PRIVATE
    FrostMonitorCoreClean
    Catch2::Catch2WithMain
)

if(MSVC)
    target_compile_options(FrostMonitorTests PRIVATE /W4 /WX /permissive- /utf-8)
endif()

include(Catch)
catch_discover_tests(FrostMonitorTests)
