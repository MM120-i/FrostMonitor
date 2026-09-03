#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "../include/frostmonitor/gamesense.hpp"

namespace {
    struct RequestLog {
        std::string path;
        nlohmann::json body;
    };

    auto defaultEvents() -> std::vector<frostmonitor::GameSenseEvent> {
        return {{.name="CPU_STATS", .min=0.0, .max=100.0}, {.name="GPU_STATS", .min=0.0, .max=100.0}};
    }

    auto testSettings(const std::string &url) -> frostmonitor::GameSenseClient::Settings {
        frostmonitor::GameSenseClient::Settings settings;
        settings.baseUrl = url;
        settings.requestTimeout = std::chrono::milliseconds{200};
        settings.initialBackoff = std::chrono::milliseconds{10};
        settings.maxBackoff = std::chrono::milliseconds{20};
        settings.heartbeatInterval = std::chrono::milliseconds{1000};
        return settings;
    }

    auto waitForState(const frostmonitor::GameSenseClient &client,
                      frostmonitor::GameSenseClient::State wanted,
                      int maxMs = 2000) -> bool {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(maxMs);

        while(std::chrono::steady_clock::now() < deadline){
            if(client.state() == wanted)
                return true;

            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }

        return client.state() == wanted;
    }

    auto waitForFailures(const frostmonitor::GameSenseClient &client,
                         std::uint64_t minFailures,
                         int maxMs = 2000) -> bool {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(maxMs);

        while(std::chrono::steady_clock::now() < deadline){
            if(client.reconnectFailures() >= minFailures)
                return true;

            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }

        return client.reconnectFailures() >= minFailures;
    }

    auto countCalls(const std::vector<RequestLog> &calls, const char *path) -> std::size_t {
        return static_cast<std::size_t>(std::count_if(calls.begin(), calls.end(),
                                                       [path](const auto &c) -> bool { return c.path == path; }));
    }

    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    class MockEngine {
    public:
        int port = 0; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

        ~MockEngine(){
            stop();
        }

        auto setup() -> bool {
            const auto handler = [this](const httplib::Request &req, httplib::Response &res) -> void {
                nlohmann::json body = nullptr;

                try{
                    body = nlohmann::json::parse(req.body);
                }
                catch(const nlohmann::json::exception &){
                    body = "unparseable";
                    res.status = 400;
                    res.set_content(R"({"error": 4, "message": "Invalid JSON"})", "application/json");
                    return;
                }

                {
                    std::scoped_lock lock(mutex_);
                    records_.push_back({.path=req.path, .body=body});

                    const auto it = failures_.find(req.path);

                    if(it != failures_.end()){
                        res.status = it->second.status;
                        res.set_content(it->second.body, "application/json");
                        return;
                    }
                }

                res.status = 200;
                res.set_content("{}", "application/json");
            };

            server_.Post("/game_metadata", handler);
            server_.Post("/bind_game_event", handler);
            server_.Post("/event", handler);
            server_.Post("/remove_game_event", handler);
            server_.Post("/remove_game", handler);

            port = server_.bind_to_any_port("127.0.0.1");

            if(port == 0)
                return false;

            listenThread_ = std::thread([this] -> void { server_.listen_after_bind(); });
            listenThread_.detach();
            return true;
        }

        auto setupOn(int desiredPort) -> bool {
            const auto handler = [this](const httplib::Request &req, httplib::Response &res) -> void {
                nlohmann::json body = nullptr;

                try{
                    body = nlohmann::json::parse(req.body);
                }
                catch(const nlohmann::json::exception &){
                    body = "unparseable";
                    res.status = 400;
                    res.set_content(R"({"error": 4, "message": "Invalid JSON"})", "application/json");
                    return;
                }

                {
                    std::scoped_lock lock(mutex_);
                    records_.push_back({.path=req.path, .body=body});

                    const auto it = failures_.find(req.path);

                    if(it != failures_.end()){
                        res.status = it->second.status;
                        res.set_content(it->second.body, "application/json");
                        return;
                    }
                }

                res.status = 200;
                res.set_content("{}", "application/json");
            };

            server_.Post("/game_metadata", handler);
            server_.Post("/bind_game_event", handler);
            server_.Post("/event", handler);
            server_.Post("/remove_game_event", handler);
            server_.Post("/remove_game", handler);

            auto fd = server_.bind_to_port("127.0.0.1", desiredPort);

            if(static_cast<int>(fd) < 0)
                return false;

            port = desiredPort;
            listenThread_ = std::thread([this] -> void { server_.listen_after_bind(); });
            listenThread_.detach();
            return true;
        }

        void failPath(const std::string &path, int status, std::string body){
            std::scoped_lock lock(mutex_);
            failures_[path] = {.status=status, .body=std::move(body)};
        }

        void clearFailures(){
            std::scoped_lock lock(mutex_);
            failures_.clear();
        }

        void stop(){
            server_.stop();
        }

        void stopAndWait(){
            server_.stop();

            while(server_.is_running())
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }

        auto calls() const -> std::vector<RequestLog> {
            std::scoped_lock lock(mutex_);
            return records_;
        }

    private:
        struct Failure {
            int status{};
            std::string body;
        };

        mutable std::mutex mutex_;
        std::vector<RequestLog> records_;
        std::map<std::string, Failure> failures_;
        httplib::Server server_;
        std::thread listenThread_;
    };

    auto cardUrl(const MockEngine &engine) -> std::string {
        return "http://127.0.0.1:" + std::to_string(engine.port);
    }

    auto deadPortUrl() -> std::string {
        return "http://127.0.0.1:19";
    }
}

TEST_CASE("registers metadata, binds both events, sends frames, unregisters on destroy"){
    MockEngine engine;
    REQUIRE(engine.setup());

    {
        auto client = std::make_unique<frostmonitor::GameSenseClient>(
            testSettings(cardUrl(engine)), "FrostMonitor", defaultEvents()
        );

        REQUIRE(waitForState(*client, frostmonitor::GameSenseClient::State::Live));

        auto calls = engine.calls();
        REQUIRE(calls.size() == 3);

        CHECK(calls[0].path == "/game_metadata");
        CHECK(calls[0].body["game"] == "FROSTMONITOR");
        CHECK(calls[0].body["game_display_name"] == "FROSTMONITOR");

        CHECK(calls[1].path == "/bind_game_event");
        CHECK(calls[1].body["event"] == "CPU_STATS");
        CHECK(calls[1].body["value_optional"] == true);
        CHECK(calls[1].body["min_value"] == 0.0);
        CHECK(calls[1].body["max_value"] == 100.0);
        CHECK(calls[1].body["handler"]["device-type"] == "screened");
        CHECK(calls[1].body["handler"]["zone"] == "one");
        CHECK(calls[1].body["handler"]["mode"] == "screen");
        CHECK(calls[1].body["handler"]["datas"][0]["has-text"] == true);
        CHECK(calls[1].body["handler"]["datas"][0]["context-frame-key"] == "line");

        CHECK(calls[2].path == "/bind_game_event");
        CHECK(calls[2].body["event"] == "GPU_STATS");

        client->send("CPU_STATS", "CPU 42C | 23%");
        client->send("GPU_STATS", "GPU 55C | 12%");

        calls = engine.calls();
        REQUIRE(calls.size() == 5);

        CHECK(calls[3].path == "/event");
        CHECK(calls[3].body["event"] == "CPU_STATS");
        CHECK(calls[3].body["data"]["frame"]["line"] == "CPU 42C | 23%");

        CHECK(calls[4].path == "/event");
        CHECK(calls[4].body["event"] == "GPU_STATS");
        CHECK(calls[4].body["data"]["frame"]["line"] == "GPU 55C | 12%");

        CHECK(client->state() == frostmonitor::GameSenseClient::State::Live);
    }

    const auto calls = engine.calls();

    CHECK(countCalls(calls, "/remove_game_event") == 2);
    CHECK(countCalls(calls, "/remove_game") == 1);
}

TEST_CASE("documented 400 error forms fail registration and the machine recovers"){
    MockEngine engine;
    REQUIRE(engine.setup());

    engine.failPath("/bind_game_event", 400, R"({"error": 4, "message": "Invalid JSON"})");

    auto client = std::make_unique<frostmonitor::GameSenseClient>(
        testSettings(cardUrl(engine)), "FrostMonitor", defaultEvents()
    );

    REQUIRE(waitForFailures(*client, 1));
    CHECK(client->reconnectAttempts() >= 1);

    engine.clearFailures();

    REQUIRE(waitForState(*client, frostmonitor::GameSenseClient::State::Live, 3000));
    CHECK(client->reconnectAttempts() >= 2);
}

TEST_CASE("5xx and malformed bodies also land in DISCONNECTED"){
    MockEngine engine;
    REQUIRE(engine.setup());
    engine.failPath("/game_metadata", 500, "not json at all");

    auto client = std::make_unique<frostmonitor::GameSenseClient>(
        testSettings(cardUrl(engine)), "FrostMonitor", defaultEvents()
    );

    REQUIRE(waitForFailures(*client, 1));
    engine.clearFailures();
    REQUIRE(waitForState(*client, frostmonitor::GameSenseClient::State::Live, 3000));
}

TEST_CASE("stays alive when the engine is down, self-heals when it appears"){
    MockEngine engine;
    REQUIRE(engine.setup());

    engine.failPath("/game_metadata", 500, "down");

    auto client = std::make_unique<frostmonitor::GameSenseClient>(
        testSettings(cardUrl(engine)), "FrostMonitor", defaultEvents()
    );

    REQUIRE(waitForFailures(*client, 1));

    engine.clearFailures();

    REQUIRE(waitForState(*client, frostmonitor::GameSenseClient::State::Live, 3000));
    CHECK(engine.calls().size() >= 3);
}

TEST_CASE("send() is a no-op while disconnected"){
    MockEngine engine;
    REQUIRE(engine.setup());

    engine.failPath("/bind_game_event", 400, R"({"error": 4, "message": "Invalid JSON"})");

    auto client = std::make_unique<frostmonitor::GameSenseClient>(
        testSettings(cardUrl(engine)), "FrostMonitor", defaultEvents()
    );

    REQUIRE(waitForState(*client, frostmonitor::GameSenseClient::State::Disconnected));

    client->send("CPU_STATS", "CPU 42C | 23%");
    client->send("GPU_STATS", "GPU 55C | 12%");

    const auto calls = engine.calls();
    CHECK(countCalls(calls, "/event") == 0);
}

TEST_CASE("backoff caps out"){
    const std::string url = deadPortUrl();

    frostmonitor::GameSenseClient::Settings settings;
    settings.baseUrl = url;
    settings.requestTimeout = std::chrono::milliseconds{10};
    settings.initialBackoff = std::chrono::milliseconds{5};
    settings.maxBackoff = std::chrono::milliseconds{10};
    settings.heartbeatInterval = std::chrono::milliseconds{1000};

    auto client = std::make_unique<frostmonitor::GameSenseClient>(
        std::move(settings), "FrostMonitor", defaultEvents()
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    CHECK(client->reconnectAttempts() >= 10);
}

TEST_CASE("destroy with the engine down never throws"){
    MockEngine engine;
    REQUIRE(engine.setup());

    auto client = std::make_unique<frostmonitor::GameSenseClient>(
        testSettings(cardUrl(engine)), "FrostMonitor", defaultEvents()
    );

    REQUIRE(waitForState(*client, frostmonitor::GameSenseClient::State::Live));
    engine.stopAndWait();
    REQUIRE_NOTHROW(client.reset());
}

TEST_CASE("register_game=false disables the client entirely"){
    frostmonitor::Config config;
    config.gamesense.registerGame = false;

    CHECK(frostmonitor::createGameSenseClient(config) == nullptr);
}

TEST_CASE("discovery file drives connect"){
    MockEngine engine;
    REQUIRE(engine.setup());

    const auto discoveryFile = std::filesystem::temp_directory_path() /
        ("frostmonitor-coreprops-" + std::to_string(engine.port) + ".json");
    {
        std::ofstream out{discoveryFile};
        out << R"({"address": ")" << cardUrl(engine) << R"("})";
    }

    frostmonitor::Config config;
    config.gamesense.discoveryFile = discoveryFile;

    auto client = frostmonitor::createGameSenseClient(config);
    REQUIRE(client != nullptr);
    REQUIRE(waitForState(*client, frostmonitor::GameSenseClient::State::Live));
    CHECK(engine.calls().size() >= 3);

    std::filesystem::remove(discoveryFile);
    engine.stop();
}

TEST_CASE("missing discovery file still returns a live client object"){
    const auto gone = std::filesystem::temp_directory_path() / "frostmonitor-does-not-exist.json";
    std::filesystem::remove(gone);

    frostmonitor::Config config;
    config.gamesense.discoveryFile = gone;

    auto client = frostmonitor::createGameSenseClient(config);
    REQUIRE(client != nullptr);
    CHECK(client->state() == frostmonitor::GameSenseClient::State::Disconnected);
}