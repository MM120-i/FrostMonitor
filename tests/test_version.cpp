#include <frostmonitor/version.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("version constants are sane"){
    CHECK(frostmonitor::appName == "FrostMonitor");
    CHECK(!frostmonitor::appVersion.empty());
    CHECK(frostmonitor::appVersion == FROSTMON_VERSION);
}
