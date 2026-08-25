#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <httplib.h>

#include "config.hpp"
#include <nlohmann/json.hpp>

namespace frostmonitor {
    struct GameSenseEvent{
        std::string name;
        double min{0.0};
        double max{100.0};
    };
    
    class GameSenseClient{
    public:
        enum class State : std::uint8_t {
            Disconnected,
            Registering,
            Live,
        };

        struct Settings {
            std::string baseUrl;
            std::filesystem::path discoveryFile{"C:/ProgramData/SteelSeries/SteelSeries Engine 3/coreProps.json"};
            std::chrono::milliseconds requestTimeout{500};
            std::chrono::milliseconds discoveryRetryDelay{200};
            int discoveryAttempts{3};
            std::chrono::milliseconds initialBackoff{1000};
            std::chrono::milliseconds maxBackoff{30000};
            std::chrono::milliseconds heartbeatInterval{10000};
        };

        GameSenseClient(Settings settings, std::string game, std::vector<GameSenseEvent> events);
        ~GameSenseClient();
        GameSenseClient(const GameSenseClient &) = delete;
        GameSenseClient(GameSenseClient &&) = delete;

        auto operator = (const GameSenseClient &) -> GameSenseClient& = delete;
        auto operator = (GameSenseClient &&) -> GameSenseClient& = delete;
        void send(std::string_view eventName, std::string line);
        static auto discoverAddress(const std::filesystem::path &file) -> std::string;

        [[nodiscard]] auto state() const noexcept -> State;
        [[nodiscard]] auto game() const noexcept -> std::string_view;
        [[nodiscard]] auto reconnectAttempts() const noexcept -> std::uint64_t;
        [[nodiscard]] auto reconnectFailures() const noexcept -> std::uint64_t;

    private:
        void runWorker(const std::stop_token &stopToken);
        auto registerOnEngine() -> bool;
        auto postSucceeds(const std::string &path, const nlohmann::json &body) -> bool;
        auto makeClient(const std::string &url) const -> std::unique_ptr<httplib::Client>;

        std::jthread worker_;
        std::condition_variable_any cv_;
        std::mutex cvMutex_;
        std::mutex httpMutex_;
        std::unique_ptr<httplib::Client> http_;
        std::atomic<State> state_{State::Disconnected};
        std::atomic<std::uint64_t> attempts_{0};
        std::atomic<std::uint64_t> failures_{0};
        std::atomic_bool everRegistered_{false};
        std::atomic_bool connectionLost_{false};
        std::atomic_bool sendWarned_{false};
        Settings settings_;
        std::string game_;
        std::vector<GameSenseEvent> events_;
    };

    auto createGameSenseClient(const Config &config) -> std::unique_ptr<GameSenseClient>;
}