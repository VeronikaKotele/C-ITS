#include "MqttClient.h"
#include "constants.h"

MqttClient::MqttClient(const std::string& id) : _id(id), _client(SERVER_ADDRESS, id) {}

void MqttClient::connect() {
    mqtt::connect_options connOpts;
    connOpts.set_clean_session(true);
    connOpts.set_automatic_reconnect(true);
    _client.connect(connOpts)->wait();
}

void MqttClient::disconnect() {
    _client.disconnect()->wait();
}
