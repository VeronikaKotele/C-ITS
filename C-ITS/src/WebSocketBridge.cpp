#include "WebSocketBridge.h"

#include <iostream>

WebSocketBridge::WebSocketBridge(uint16_t port)
    : _port(port) {

    _server.clear_access_channels(websocketpp::log::alevel::all);
    _server.clear_error_channels(websocketpp::log::elevel::all);

    _server.init_asio();

    // Allow immediate rebinding of the port after shutdown/restart.
    // This sets the SO_REUSEADDR option on the acceptor so a restart
    // while a client is aggressively reconnecting is less likely to fail.
    _server.set_reuse_addr(true);

    _server.set_open_handler([this](ConnectionHandle hdl) {
        if (!_running) {
            websocketpp::lib::error_code ec;
            _server.close(hdl, websocketpp::close::status::going_away, "Server not running", ec);
            return;
        }
        std::lock_guard<std::mutex> lock(_connectionsMutex);
        _connections.insert(hdl);
        std::cout << "[WS] Client connected\n";
        });

    _server.set_close_handler([this](ConnectionHandle hdl) {
        std::lock_guard<std::mutex> lock(_connectionsMutex);
        _connections.erase(hdl);
        std::cout << "[WS] Client disconnected\n";
        });

    _server.set_fail_handler([this](ConnectionHandle hdl) {
        std::lock_guard<std::mutex> lock(_connectionsMutex);
        _connections.erase(hdl);
        std::cerr << "[WS] Client failed\n";
        });
}

void WebSocketBridge::start() {
    if (_running.exchange(true)) {
        return;
    }

    websocketpp::lib::error_code ec;

    _server.listen(_port, ec);
    if (ec) {
        std::cerr << "[WS] Listen failed: " << ec.message() << "\n";
        _running = false;
        return;
    }

    _server.start_accept(ec);
    if (ec) {
        std::cerr << "[WS] Start accept failed: " << ec.message() << "\n";
        _running = false;
        return;
    }

    _serverThread = std::thread([this]() {
        std::cout << "[WS] Server started on ws://localhost:" << _port << "\n";

        try {
            _server.run();
        }
        catch (const std::exception& ex) {
            std::cerr << "[WS] Server exception: " << ex.what() << "\n";
        }

        std::cout << "[WS] Server run loop exited\n";
        });
}

void WebSocketBridge::stop() {
    if (!_running.exchange(false)) {
        return;
    }

    websocketpp::lib::error_code ec;
    _server.stop_listening(ec);
    if (ec) {
        std::cerr << "[WS] stop_listening error: " << ec.message() << "\n";
    }

    std::vector<ConnectionHandle> connections;
    {
        std::lock_guard<std::mutex> lock(_connectionsMutex);
        connections.assign(_connections.begin(), _connections.end());
        _connections.clear();
    }

    for (const auto& hdl : connections) {
        websocketpp::lib::error_code closeEc;
        _server.close(
            hdl,
            websocketpp::close::status::going_away,
            "Server shutdown",
            closeEc
        );

        if (closeEc) {
            std::cerr << "[WS] close error: " << closeEc.message() << "\n";
        }
    }

    _server.stop();

    if (_serverThread.joinable()) {
        _serverThread.join();
    }

    std::cout << "[WS] Server stopped\n";
}

void WebSocketBridge::broadcastJson(const nlohmann::json& json) {
    const std::string message = json.dump();

    std::lock_guard<std::mutex> lock(_connectionsMutex);

    for (const auto& hdl : _connections) {
        websocketpp::lib::error_code ec;

        _server.send(
            hdl,
            message,
            websocketpp::frame::opcode::text,
            ec
        );

        if (ec) {
            std::cerr << "[WS] Send error: " << ec.message() << "\n";
        }
    }
}