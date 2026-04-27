#include "Vehicle.h"
#include <mqtt/async_client.h>
#include <iostream>
#include <format>
#include "constants.h"
#include "utils.h"
#include "etsi_common.pb.h"
#include "cam.pb.h"
#include "srem.pb.h"
#include "ssem.pb.h"
#include "InterfacesTranslator.h"

Vehicle::Vehicle(uint32_t id, VehicleType type)
    : MqttClient(id),
    _vehicleType(type),
    _vehicleRole(ItsVehicleRoleFromVehicleType(type)),
    _stationType(ItsStationTypeFromVehicleType(type)),
    _callback(*this)
{
    _client.set_callback(_callback);
}

std::string Vehicle::name() const {
    return "Vehicle " + _vehicleType + " no " + std::to_string(_id);
}

std::string Vehicle::to_json() const {
    return R"({"vehicle_id":{}})" + std::to_string(_id);
}

void Vehicle::sendSpeedStatus() {
	const auto topic = std::format("its/vehicle/{}/cam", _id);

    its::Cam cam;

    auto* header = cam.mutable_header();
    header->set_protocol_version(1);
    header->set_message_type(its::MESSAGE_TYPE_CAM);
    header->set_station_id(_id);

    auto* ccam = cam.mutable_cam();
	ccam->set_generation_delta_time(currentGenerationDeltaTime());
    
	auto* basicCam = ccam->mutable_basic_container();
    basicCam->set_station_type(this->_stationType);

	auto* position = basicCam->mutable_reference_position();
    position->set_latitude(51.2277);  // todo: use dynamic location
    position->set_longitude(6.7735);

	auto* highFrequencyCam = ccam->mutable_high_frequency_container();
	auto* highFrequencyBasicCam = highFrequencyCam->mutable_basic_vehicle_container_high_frequency();
	highFrequencyBasicCam->set_speed(rand() % 200);
	highFrequencyBasicCam->set_heading(rand() % 360);

    auto* lowFrequencyCam = ccam->mutable_low_frequency_container();
    auto* lowFrequencyBasicVehicleCam = lowFrequencyCam->mutable_basic_vehicle_container_low_frequency();
    lowFrequencyBasicVehicleCam->set_vehicle_role(this->_vehicleRole);

    auto* specialVehicleCam = ccam->mutable_special_vehicle_container();
    auto* emergencyCam = specialVehicleCam->mutable_emergency_container();
	auto* light_bar_siren_in_use = emergencyCam->mutable_light_bar_siren_in_use();
	light_bar_siren_in_use->set_light_bar_activated(true);
    light_bar_siren_in_use->set_siren_activated(false);

    std::string payload;
    cam.SerializeToString(&payload);

    try {
        std::cout << "[" << name() << "] Sending CAM: " 
            << "generation_delta_time = " << ccam->generation_delta_time() << "\n";

        send(topic, payload);

        std::cout << "[" << name() << "] Sent CAM: "
            << "generation_delta_time = " << ccam->generation_delta_time() << "\n";
    }
    catch (const mqtt::exception& ex) {
        std::cerr << "[" << name() << "] Failed to publish CAM: "
            << "generation_delta_time = " << ccam->generation_delta_time()
            << ex.what() << "\n";
    }
}

void Vehicle::requestPriority() {
    const auto topic = std::format("its/vehicle/{}/srem", _id);

    its::Srem srem;
    auto* header = srem.mutable_header();
    header->set_protocol_version(1);
    header->set_message_type(its::MESSAGE_TYPE_CAM);
    header->set_station_id(_id);

    srem.set_generation_delta_time(currentGenerationDeltaTime());

    auto* requestor = srem.mutable_requestor();
	requestor->set_station_id(_id);
	requestor->set_role(this->_vehicleRole);

    auto* position = requestor->mutable_reference_position();
    position->set_latitude(51.2277);  // todo: use dynamic location
    position->set_longitude(6.7735);

	auto* request = srem.mutable_request();
	request->set_request_id(rand());
	request->set_intersection_id(123); // todo: use real intersection id
	request->set_eta_seconds(30);

    std::string payload;
    srem.SerializeToString(&payload);
    
    try {
        std::cout << "[" << name() << "] Sending SREM: "
            << "generation_delta_time = " << srem.generation_delta_time()
            << ", request_id = " << request->request_id() << "\n";

        send(topic, payload);

        std::cout << "[" << name() << "] Sent SREM: "
            << "generation_delta_time = " << srem.generation_delta_time()
            << ", request_id = " << request->request_id() << "\n";
    }
    catch (const mqtt::exception& ex) {
        std::cerr << "[" << name() << "] Failed to publish SREM: "
            << "generation_delta_time = " << srem.generation_delta_time()
            << ", request_id = " << request->request_id()
            << ex.what() << "\n";
    }
}

void Vehicle::subscribeToListenSsem() {
    subscribe(std::format( "its/rsu/+/ssem/{}", _id));
}

Vehicle::Callback::Callback(Vehicle& owner) : _owner(owner) {}

void Vehicle::Callback::message_arrived(mqtt::const_message_ptr msg) {
    const std::string topic = msg->get_topic();
    const std::string payload = msg->to_string();

    std::cout << "[" << _owner.name() << "] message arrived: " << topic << "\n";

    try {
        if (topic.find("/ssem") != std::string::npos) {
            its::Ssem ssem;

            if (!ssem.ParseFromString(payload)) {
                std::cerr << "[" << _owner.name() << "] Failed to parse SSEM from topic: "
                    << topic << "\n";
                return;
            }

            _owner.handleSsem(ssem);
            return;
        }

        std::cout << "[" << _owner.name() << "] Ignored unsupported topic: "
            << topic << "\n";
    }
    catch (const std::exception& ex) {
        std::cerr << "[" << _owner.name() << "] Exception in message_arrived: "
            << ex.what() << "\n";
    }
}

void Vehicle::handleSsem(const its::Ssem& ssem) {
    try {
        std::cout << "[" << name() << "] received SSEM: "
			<< "requestor_id = " << ssem.requestor_station_id()
			<< ", request_id = " << ssem.request_id()
            << ", status = " << ItsEnumValueToString(ssem.status()) << "\n";

        priorityGranted(ssem.status() == its::RequestStatus::REQUEST_STATUS_GRANTED);
    }
    catch (const std::exception& ex) {
        std::cerr << "[" << name() << "] Exception in message_arrived: "
            << ex.what() << "\n";
    }
}


void Vehicle::priorityGranted(bool granted) {
    std::cout << "[" << name() << "] priority request was " << (granted ? "granted" : "denied") << "\n";
    //todo: implement behavior change based on priority grant result, e.g. if granted, start crossing intersection, if denied, slow down and wait for next opportunity
}