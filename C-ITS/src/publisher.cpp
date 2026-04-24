#include <mqtt/async_client.h>
#include <iostream>
#include <format>
#include "constants.h"
#include "srem.pb.h"

class Vehicle {
public:
    Vehicle(int id) : _id(id), _client(SERVER_ADDRESS, id) {}

    std::string to_json() const {
        return std::format(R"({"vehicle_id":{})", _id);
    }

    int connect() {
        mqtt::connect_options connOpts;
        connOpts.set_clean_session(true);
        connOpts.set_automatic_reconnect(true);
        try {
            _client.connect(connOpts)->wait();
            return 0;
        }
        catch (const mqtt::exception& e) {
            std::cerr << "Connection error: " << e.what() << std::endl;
            return -1;
        }
	}

    int sendSpeedStatus() {
        its::SREM srem;
        srem.set_vehicle_id(42);
        srem.set_vehicle_type(its::VehicleType::CAR);
        srem.set_latitude(51.2277);
        srem.set_longitude(6.7735);
        srem.set_speed(rand() % 200);
        srem.set_priority(false);

        std::string payload;
        srem.SerializeToString(&payload);
        auto msg = mqtt::make_message("its/vehicle/42/srem", payload);
        //std::string payload = std::format(R"({{"vehicle_id":{},"speed":{}}})", _id, _speed);
        //auto msg = mqtt::make_message(std::format("its/vehicle/{}/srem", _id), payload);
        msg->set_qos(1);

        try {
            _client.publish(msg)->wait();
        }
        catch (const mqtt::exception& e) {
            std::cerr << "Error sending speed status: " << e.what() << std::endl;
        }

        return 0;
    }

    int requestPriority() {
        its::SREM srem;
        srem.set_vehicle_id(42);
		srem.set_vehicle_type("car");
        srem.set_latitude(51.2277);
        srem.set_longitude(6.7735);
        srem.set_speed(40.5f);
        srem.set_priority(true);

        std::string payload;
        srem.SerializeToString(&payload);
        auto msg = mqtt::make_message("its/vehicle/42/srem", payload);

        //std::string payload = std::format(R"({{"vehicle_id":{},"priority":true}})", _id);
        //auto msg = mqtt::make_message(std::format("its/vehicle/{}/srem", _id), payload);
        msg->set_qos(1);

        try {
            _client.publish(msg)->wait();
			_client.disconnect()->wait();
        }
        catch (const mqtt::exception& e) {
            std::cerr << "Error sending priority request: " << e.what() << std::endl;
        }

        return 0;
    }

    int disconnect() {
        try {
            _client.disconnect()->wait();
            return 0;
        }
        catch (const mqtt::exception& e) {
            std::cerr << "Disconnection error: " << e.what() << std::endl;
            return -1;
        }
	}

private:
    int _id;
	mqtt::async_client _client;
};
