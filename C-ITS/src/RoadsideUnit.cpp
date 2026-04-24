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
    std::cout << "Received message with topic: " << msg->get_topic() << std::endl;

    std::string payload = msg->to_string();
    its::SREM srem;
    if (srem.ParseFromString(payload)) {
        std::cout << "\tvehicle_id: " << srem.vehicle_id() << std::endl;
		std::cout << "\tvehicle_type: " << srem.vehicle_type() << std::endl;
        std::cout << "\tlatitude: " << srem.latitude() << std::endl;
        std::cout << "\tlongitude: " << srem.longitude() << std::endl;
        std::cout << "\tspeed: " << srem.speed() << std::endl;
        std::cout << "\tpriority: " << srem.priority() << std::endl << std::endl;
    }
    else {
        std::cerr << "Failed to parse Protobuf message!" << std::endl;
    }
}
