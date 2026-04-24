#pragma once

#include <mqtt/async_client.h>

class MqttClient {
public:
    MqttClient(const std::string& id);

    void connect();

    void disconnect();

protected:
    std::string _id;
    mqtt::async_client _client;
};
