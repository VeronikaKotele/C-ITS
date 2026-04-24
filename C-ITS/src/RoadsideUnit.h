#pragma once

#include "MqttClient.h"
#include <mqtt/async_client.h>

class RoadsideUnit : public MqttClient {
public:
    class Callback : public virtual mqtt::callback {
    public:
        void message_arrived(mqtt::const_message_ptr msg) override;
    };

    RoadsideUnit(const std::string& id);

    void subscribeToListenSREM();

private:
	Callback _callback;
};
