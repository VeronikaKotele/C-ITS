#include <mqtt/async_client.h>
#include <iostream>
#include "constants.h"
#include "srem.pb.h"

class RoadsideUnit {
private:
    class callback : public virtual mqtt::callback {
    public:
        void message_arrived(mqtt::const_message_ptr msg) override {
            std::cout << "Topic: " << msg->get_topic() << std::endl;
            std::cout << "Message: " << msg->to_string() << std::endl;

            std::string payload = msg->to_string();
            its::SREM srem;
            if (srem.ParseFromString(payload)) {
                std::cout << "vehicle_id: " << srem.vehicle_id() << std::endl;
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
    };

public:
	RoadsideUnit(const std::string& id) : _id(id), _client(SERVER_ADDRESS, id) {}

    int connect() {
        callback cb;
        _client.set_callback(cb);

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

    int subscribeToListenSREM() {
        try {
            client.subscribe("its/vehicle/+/srem", 1)->wait();

            std::cout << "Listening..." << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(1));

			client.disconnect()->wait();
        }
        catch (const mqtt::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
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
    std::string _id;
	mqtt::async_client _client;
};