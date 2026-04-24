#include "Vehicle.h"
#include <mqtt/async_client.h>
#include <iostream>
#include <format>
#include "constants.h"
#include "srem.pb.h"
#include "VehicleType.h"

Vehicle::Vehicle(const std::string& id) : MqttClient(id) {}

std::string Vehicle::to_json() const {
    return R"({"vehicle_id":{}})" + _id;
}

void Vehicle::sendSpeedStatus() {
    its::SREM srem;
    srem.set_vehicle_id(42);
    srem.set_vehicle_type(VehicleType::CAR);
    srem.set_latitude(51.2277);
    srem.set_longitude(6.7735);
    srem.set_speed(rand() % 200);
    srem.set_priority(false);

    std::string payload;
    srem.SerializeToString(&payload);
    auto msg = mqtt::make_message(std::format("its/vehicle/{}/srem", _id), payload);
    msg->set_qos(1);

    _client.publish(msg)->wait();
}

void Vehicle::requestPriority() {
    its::SREM srem;
    srem.set_vehicle_id(42);
	srem.set_vehicle_type(VehicleType::CAR);
    srem.set_latitude(51.2277);
    srem.set_longitude(6.7735);
    srem.set_speed(40.5f);
    srem.set_priority(true);

    std::string payload;
    srem.SerializeToString(&payload);
    auto msg = mqtt::make_message(std::format("its/vehicle/{}/srem", _id), payload);
    msg->set_qos(1);

    _client.publish(msg)->wait();
}
