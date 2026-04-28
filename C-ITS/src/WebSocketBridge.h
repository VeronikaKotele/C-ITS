#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <nlohmann/json.hpp>

#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <thread>

class WebSocketBridge {
public:
    using Server = websocketpp::server<websocketpp::config::asio>;
    using ConnectionHandle = websocketpp::connection_hdl;

    explicit WebSocketBridge(uint16_t port);

    void start();
    void stop();

    void broadcastJson(const nlohmann::json& json);

private:
    struct ConnectionCompare {
        bool operator()(const ConnectionHandle& a, const ConnectionHandle& b) const {
            return a.owner_before(b);
        }
    };

    uint16_t _port;
    Server _server;
    std::thread _serverThread;
    std::atomic<bool> _running{ false };

    std::mutex _connectionsMutex;
    std::set<ConnectionHandle, ConnectionCompare> _connections;
};