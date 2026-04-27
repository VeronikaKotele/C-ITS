#pragma once

#include <mqtt/async_client.h>
#include <numeric>

class MqttClient {
public:
    MqttClient(uint32_t id);

    void connect();

    void disconnect();

	void send(const std::string& topic, const std::string& payload);

    void subscribe(const std::string& topic);

protected:
    uint32_t _id;
    mqtt::async_client _client;
};
