#include "../include/frostmonitor/gamesense.hpp"

#include <spdlog/spdlog.h>

#include <charconv>
#include <fstream>
#include <sstream>
#include <utility>

namespace frostmonitor {
    const constexpr int PORT_LIMIT = 65535;

    namespace {
        constexpr auto kRemoveGameEventPath = "/remove_game_event";
        constexpr auto kRemoveGamePath = "/remove_game";

        auto readJsonFile(const std::filesystem::path &file) -> nlohmann::json {
            std::ifstream stream{file};
            nlohmann::json value;
            stream >> value;
            return value;
        }

        auto splitHostPort(std::string_view url) -> std::pair<std::string, int> {
            const auto scheme = url.find("://");

            if(scheme != std::string_view::npos)
                url = url.substr(scheme + 3);

            const auto colon = url.rfind(':');

            if(colon == std::string_view::npos || colon + 1 >= url.size())
                return {};

            const std::string_view portText = url.substr(colon + 1);
            std::string host{url.substr(0, colon)};
            int port = 0;
            const auto parsed = std::from_chars(portText.data(), portText.data() + portText.size(), port);

            if(host.empty() || parsed.ec != std::errc{} || parsed.ptr != portText.data() + portText.size() || port <= 0 || port > PORT_LIMIT)
                return {};

            return {std::move(host), port};
        }
    }

    auto discoverAddress(const std::filesystem::path &file) -> std::string {
        try{
            const nlohmann::json props = readJsonFile(file);
            return props.value("address", std::string{});
        }
        catch(const nlohmann::json::exception &){
            return {};
        }
    }

    auto GameSenseClient::discoverAddress(const std::filesystem::path &file) -> std::string {
        const std::string address = frostmonitor::discoverAddress(file);

        if(address.empty())
            spdlog::debug("GameSense discovery failed: {} is missing or lacks an address", file.string());
        else
            spdlog::debug("GameSense discovery: {} -> {}", file.string(), address);

        return address;
    }

    auto createGameSenseClient(const Config &config) -> std::unique_ptr<GameSenseClient> {
        if(!config.gamesense.registerGame)
            return nullptr;

        GameSenseClient::Settings settings;
        settings.baseUrl = config.gamesense.address;
        settings.discoveryFile = config.gamesense.discoveryFile;

        for(int attempt = 0; settings.baseUrl.empty() && attempt < settings.discoveryAttempts; attempt++){
            if(attempt > 0)
                std::this_thread::sleep_for(settings.discoveryRetryDelay);

            settings.baseUrl = GameSenseClient::discoverAddress(settings.discoveryFile);
        }

        if(settings.baseUrl.empty())
            spdlog::warn("GameSense not reachable at startup - will keep trying in the background");

        std::vector<GameSenseEvent> events {
            {config.cpuEvent.name, config.cpuEvent.min, config.cpuEvent.max},
            {config.gpuEvent.name, config.gpuEvent.min, config.gpuEvent.max},
        };
        
        return std::make_unique<GameSenseClient>(std::move(settings), config.appName, std::move(events));
    }

    GameSenseClient::GameSenseClient(Settings settings, std::string game, std::vector<GameSenseEvent> events)
        : settings_(std::move(settings)), game_(std::move(game)), events_(std::move(events)) {
        worker_ = std::jthread([this](const std::stop_token &stopToken) {
            runWorker(stopToken);
        });
    }

    GameSenseClient::~GameSenseClient(){
        worker_.request_stop();

        if(worker_.joinable())
            worker_.join();

        if(!everRegistered_.load(std::memory_order_relaxed)){
            spdlog::info("GameSense: Nothing was registered, skipping unregister");
            return;
        }

        try{
            std::lock_guard lock(httpMutex_);

            for(const auto &event : events_){
                if(http_ == nullptr)
                    break;

                auto response = http_->Post(kRemoveGameEventPath,
                    nlohmann::json{{"game", game_}, {"event", event.name}}.dump(),
                    "application/json");
            }

            if(http_ != nullptr)
                http_->Post(kRemoveGamePath,
                    nlohmann::json{{"game", game_}}.dump(),
                    "application/json");

            everRegistered_.store(false, std::memory_order_relaxed);
            spdlog::info("GameSense: Unregistered '{}'", game_);

        }
        catch(const std::exception& e){
            spdlog::warn("GameSense unregister failed: {}", e.what());
        }
    }

    auto GameSenseClient::state() const noexcept -> State {
        return state_.load(std::memory_order_relaxed);
    }

    auto GameSenseClient::game() const noexcept -> std::string_view {
        return game_;
    }

    auto GameSenseClient::reconnectAttempts() const noexcept -> std::uint64_t {
        return attempts_.load(std::memory_order_relaxed);
    }

    auto GameSenseClient::reconnectFailures() const noexcept -> std::uint64_t {
        return failures_.load(std::memory_order_relaxed);
    }

    void GameSenseClient::runWorker(const std::stop_token &stopToken){
        auto backOff = settings_.initialBackoff;
        bool wasLive = false;
        bool firstCycle = true;

        // Loop body is too large, shorten it
        while(!stopToken.stop_requested()){
            if(!firstCycle){
                std::unique_lock lock(cvMutex_);
                
                cv_.wait_for(lock, stopToken, backOff, [] {
                    return false;
                });

                if(stopToken.stop_requested())
                    break;
            }

            firstCycle = false;
            std::string url = settings_.baseUrl;

            if(url.empty()){
                url = discoverAddress(settings_.discoveryFile);

                if(url.empty()){
                    attempts_++;
                    failures_++;

                    if(wasLive){
                        spdlog::warn("GameSense connection lost - retrying ({} failures)", failures_.load());
                        wasLive = false;
                    }

                    backOff = std::min(backOff * 2, settings_.maxBackoff);
                    continue;
                }
            }

            {
                std::lock_guard lock(httpMutex_);
                http_ = makeClient(url);
            }

            if(!http_){
                attempts_++;
                failures_++;
                backOff = std::min(backOff * 2, settings_.maxBackoff);
                continue;
            }

            state_.store(State::Registering, std::memory_order_relaxed);

            if(!registerOnEngine()){
                attempts_++;
                failures_++;
                backOff = std::min(backOff * 2, settings_.maxBackoff);
                state_.store(State::Disconnected, std::memory_order_relaxed);
                
                if(wasLive)
                    spdlog::warn("GameSense re-registration failed ({} failures)", failures_.load());
                else
                    spdlog::debug("GameSense registration attempt {} failed", attempts_.load());

                wasLive = false;
                continue;
            }

            // LIVE
            attempts_++;
            backOff = settings_.initialBackoff;
            wasLive = true;
            everRegistered_.store(true, std::memory_order_relaxed);
            state_.store(State::Live, std::memory_order_relaxed);
            spdlog::info("GameSense LIVE: game '{}' at '{}'", game_, url);

            while(state_.load(std::memory_order_relaxed) == State::Live && !stopToken.stop_requested()){
                bool kicked = false;

                {
                    std::unique_lock lock(cvMutex_);
                    kicked = cv_.wait_for(lock, stopToken, settings_.heartbeatInterval, [this]{
                        return connectionLost_.load(std::memory_order_relaxed);
                    });
                }

                if(stopToken.stop_requested())
                    break;

                if(kicked){
                    spdlog::warn("GameSense connection lost - reconnecting");
                    break;
                }

                if(!registerOnEngine()){
                    spdlog::warn("GameSense heartbeat failed - reconnecting");
                    break;
                }
            }

            if(state_.load(std::memory_order_relaxed) == State::Live){
                state_.store(State::Disconnected, std::memory_order_relaxed);
                connectionLost_.store(false, std::memory_order_relaxed);
            }
        }
    }

    auto GameSenseClient::makeClient(const std::string &url) -> std::unique_ptr<httplib::Client> {
        auto [host, port] = splitHostPort(url);

        if(host.empty() || port <= 0 || port > PORT_LIMIT)
            return nullptr;

        auto client = std::make_unique<httplib::Client>(host, port);
        const auto ms = settings_.requestTimeout.count();
        const auto sec = static_cast<time_t>(ms / 1000);
        const auto usec = static_cast<time_t>((ms % 1000) * 1000);

        client->set_connection_timeout(sec, usec);
        client->set_read_timeout(sec, usec);
        client->set_write_timeout(sec, usec);

        return client;
    }

    auto GameSenseClient::registerOnEngine() -> bool {
        const nlohmann::json metadata {
            {"game", game_},
            {"game_display_name", game_},
        };

        if(!postSucceeds("/game_metadata", metadata))
            return false;

        for(const auto &event : events_){
            const nlohmann::json bind {
                {"game", game_},
                {"event", event.name},
                {"value_optional", true},
                {"min_value", event.min},
                {"max_value", event.max},
                {"handler", nlohmann::json::object({
                    {"device-type", "screened"},
                    {"zone", "one"},
                    {"mode", "screen"},
                    {"datas", nlohmann::json::array({nlohmann::json::object({
                        {"has-text", true},
                        {"context-frame-key", "line"}
                    })})}
                })}
            };

            if(!postSucceeds("/bind_game_event", bind))
                return false;
        }

        return true;
    }

    auto GameSenseClient::postSucceeds(const std::string &path, const nlohmann::json &body) -> bool {
        if(http_ == nullptr)
            return false;

        std::lock_guard lock(httpMutex_);
        auto response = http_->Post(path, body.dump(), "application/json");

        if(response == nullptr){
            spdlog::debug("GameSense HTTP connection error on {}", path);
            return false;
        }

        if(response->status < 200 || response->status >= 300){
            spdlog::debug("GameSense {} -> HTTP {}: {}", path, response->status, response->body.substr(0, 200));
            return false;
        }

        return true;
    }

    void GameSenseClient::send(std::string_view eventName, std::string line){
        if(state_.load(std::memory_order_relaxed) != State::Live)
            return;

        nlohmann::json frame {
            {"game", game_},
            {"event", std::string(eventName)},
            {"data", nlohmann::json::object({
                {"frame", nlohmann::json::object({
                    {"line", std::move(line)}
                })}
            })}
        };

        bool sent = false;

        {
            std::lock_guard lock(httpMutex_);

            if(http_ != nullptr){
                auto response = http_->Post("/event", frame.dump(), "application/json");

                if(response == nullptr){
                    connectionLost_.store(true, std::memory_order_relaxed);
                    cv_.notify_all();
                }
                else if(response->status >= 200 && response->status < 300){
                    sent = true;
                }
            }
        }

        if(sent){
            sendWarned_.store(false, std::memory_order_relaxed);
            return;
        }

        if(!sendWarned_.exchange(true, std::memory_order_relaxed))
            spdlog::warn("GameSense frame send failed");
    }
}