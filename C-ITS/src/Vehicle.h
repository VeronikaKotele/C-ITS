#pragma once

#include "MqttClient.h"

class Vehicle : public MqttClient {
public:
    Vehicle(const std::string& id);

    std::string to_json() const;

    void sendSpeedStatus();

    void requestPriority();
};
