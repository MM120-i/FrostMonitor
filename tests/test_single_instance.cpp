#include <catch2/catch_test_macros.hpp>
#include "../include/frostmonitor/single_instance.hpp"

TEST_CASE("second guard on the same name is not primary") {
    frostmonitor::SingleInstanceGuard first(L"FrostMonitorTestSingleInstance");
    REQUIRE(first.isPrimary());

    frostmonitor::SingleInstanceGuard second(L"FrostMonitorTestSingleInstance");
    CHECK(!second.isPrimary());
    CHECK(first.isPrimary());
}