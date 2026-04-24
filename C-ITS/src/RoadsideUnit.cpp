#include "RoadsideUnit.h"
#include <mqtt/async_client.h>
#include <iostream>
#include "constants.h"
#include "srem.pb.h"

RoadsideUnit::RoadsideUnit(const std::string& id) : MqttClient(id), _callback(){
    _client.set_callback(_callback);
}

void RoadsideUnit::subscribeToListenSREM() {
    _client.subscribe("its/vehicle/+/srem", 1)->wait();
}

void RoadsideUnit::Callback::message_arrived(mqtt::const_message_ptr msg) {
    std::cout << "Topic: " << msg->get_topic() << std::endl;
    std::cout << "Message: " << msg->to_string() << std::endl;

    std::string payload = msg->to_string();
    its::SREM srem;
    if (srem.ParseFromString(payload)) {
        std::cout << "vehicle_id: " << srem.vehicle_id() << std::endl;
		std::cout << "vehicle_type: " << srem.vehicle_type() << std::endl;
        std::cout << "latitude: " << srem.latitude() << std::endl;
        std::cout << "longitude: " << srem.longitude() << std::endl;
        std::cout << "speed: " << srem.speed() << std::endl;
        std::cout << "priority: " << srem.priority() << std::endl;
    }
    else {
        std::cerr << "Failed to parse Protobuf message!" << std::endl;
        std::cout << "Payload: " << payload << std::endl;
    }
}
