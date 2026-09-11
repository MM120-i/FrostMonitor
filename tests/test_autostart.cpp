#include <catch2/catch_test_macros.hpp>
#include "../include/frostmonitor/autostart.hpp"

TEST_CASE("autostart command quotes exe and config paths") {
    CHECK(frostmonitor::buildAutoStartCommand(L"C:\\Tools\\FrostMonitor.exe", L"C:\\Tools\\config.json")
        == L"\"C:\\Tools\\FrostMonitor.exe\" --tray \"C:\\Tools\\config.json\"");
}

TEST_CASE("autostart command handles spaces in paths") {
    CHECK(frostmonitor::buildAutoStartCommand(L"C:\\My Tools\\FrostMonitor.exe", L"C:\\My Tools\\cfg\\config.json")
        == L"\"C:\\My Tools\\FrostMonitor.exe\" --tray \"C:\\My Tools\\cfg\\config.json\"");
}