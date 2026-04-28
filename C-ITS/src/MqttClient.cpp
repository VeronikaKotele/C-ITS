#include "MqttClient.h"
#include "constants.h"

MqttClient::MqttClient(uint32_t id) : _id(id), _client(BROKER_ADDRESS, std::to_string(id)) {}

void MqttClient::connect() {
    mqtt::connect_options connOpts;
    connOpts.set_clean_session(true);
    connOpts.set_automatic_reconnect(true);
    _client.connect(connOpts)->wait();
}

void MqttClient::disconnect() {
    _client.disconnect()->wait();
}

void MqttClient::send(const std::string& topic, const std::string& payload) {
    auto message = mqtt::make_message(topic, payload);
    message->set_qos(1);
    message->set_retained(false);
    try {
        _client.publish(message);
    }
    catch (const mqtt::exception& ex) {
        std::cerr << "Failed to publish message: " << ex.what() << "\n";
	}
}

void MqttClient::subscribe(const std::string& topic) {
    try {
        _client.subscribe(topic, 1)->wait();
    }
    catch (const mqtt::exception& ex) {
        std::cerr << "Failed to subscribe to topic: " << ex.what() << "\n";
    }
}