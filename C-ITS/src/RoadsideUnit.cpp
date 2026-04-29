#include "RoadsideUnit.h"
#include <mqtt/async_client.h>
#include <iostream>
#include <thread>
#include <chrono>
#include "constants.h"
#include "etsi_common.pb.h"
#include "cam.pb.h"
#include "srem.pb.h"
#include "ssem.pb.h"
#include "spatem.pb.h"
#include "InterfacesTranslator.h"
#include "utils.h"

RoadsideUnit::RoadsideUnit(uint32_t id, SpawnLocation location, WebSocketBridge& wsBridge) : MqttClient(id), _callback(*this), _wsBridge(wsBridge) {
    _client.set_callback(_callback);
	_state.station_id = id;
	_state.latitude = location.latitude;
	_state.longitude = location.longitude;
	_state.trafficlight_phase = its::TrafficLightPhase::TRAFFICLIGHT_PHASE_RED;
	_state.remaining_seconds = 5;
	_state.generation_delta_time = currentGenerationDeltaTime();
}

void RoadsideUnit::listenVehiclesUpdate() {
    subscribe("its/vehicle/+/cam");
}

void RoadsideUnit::listenPriorityRequests() {
    subscribe("its/vehicle/+/srem");
}

RoadsideUnit::Callback::Callback(RoadsideUnit& owner) : _owner(owner) {}

void RoadsideUnit::Callback::message_arrived(mqtt::const_message_ptr msg) {
    const std::string topic = msg->get_topic();
    const std::string payload = msg->to_string();

    std::cout << "[RSU] message arrived: " << topic << "\n";

    try {
        if (topic.ends_with("/cam")) {
            its::Cam cam;

            if (!cam.ParseFromString(payload)) {
                std::cerr << "[RSU] Failed to parse CAM from topic: "
                    << topic << "\n";
                return;
            }

            _owner.handleCam(cam);
            return;
        }

        if (topic.ends_with("/srem")) {
            its::Srem srem;

            if (!srem.ParseFromString(payload)) {
                std::cerr << "[RSU] Failed to parse SREM from topic: "
                    << topic << "\n";
                return;
            }

            _owner.handleSrem(srem);
            return;
        }

        std::cout << "[RSU] Ignored unsupported topic: "
            << topic << "\n";
    }
    catch (const std::exception& ex) {
        std::cerr << "[RSU] Exception in message_arrived: "
            << ex.what() << "\n";
    }
}

void RoadsideUnit::handleCam(const its::Cam& cam) {
    if (!cam.has_header()) {
        std::cerr << "[RSU] CAM ignored: missing header\n";
        return;
    }

    if (!cam.has_cam()) {
        std::cerr << "[RSU] CAM ignored: missing cam parameters\n";
        return;
    }

    const auto& header = cam.header();
    const auto& params = cam.cam();

    const uint32_t requestorId = header.station_id();

    if (!params.has_basic_container()) {
        std::cerr << "[RSU] CAM ignored: missing basic container\n";
        return;
    }

    const auto& basic = params.basic_container();

    VehicleState state{};
    state.station_id = requestorId;
    state.station_type = basic.station_type();
    state.generation_delta_time = params.generation_delta_time();

    if (basic.has_reference_position()) {
        state.latitude = basic.reference_position().latitude();
        state.longitude = basic.reference_position().longitude();
    }

    if (params.has_special_vehicle_container() &&
        params.special_vehicle_container().has_emergency_container()) {
        state.vehicle_role = its::VEHICLE_ROLE_EMERGENCY;
	}
    else if (params.has_special_vehicle_container() &&
        params.special_vehicle_container().has_public_transport_container()) {
        state.vehicle_role = its::VEHICLE_ROLE_PUBLIC_TRANSPORT;
    }
    else if (params.has_low_frequency_container() &&
        params.low_frequency_container().has_basic_vehicle_container_low_frequency()) {
		auto lowFreqBasic = params.low_frequency_container().basic_vehicle_container_low_frequency();
        state.vehicle_role = lowFreqBasic.vehicle_role();
    }
    else {
        state.vehicle_role = its::VEHICLE_ROLE_DEFAULT;
    }

    if (params.has_high_frequency_container() &&
        params.high_frequency_container().has_basic_vehicle_container_high_frequency()) {
        auto highFreqBasic = params.high_frequency_container().basic_vehicle_container_high_frequency();
        state.heading = highFreqBasic.heading();
		state.speed = highFreqBasic.speed();
	}

    _vehicleStates[requestorId] = state;

    _wsBridge.broadcastJson({
        {"type", "cam"},
        {"stationId", cam.header().station_id()},
        {"stationType", ItsEnumValueToString(state.station_type)},
        {"role", ItsEnumValueToString(state.vehicle_role)},
        {"lat", state.latitude},
        {"lon", state.longitude},
        {"speed", state.speed},
        {"heading", state.heading},
        {"timestampMs", currentTimestampMs()}
    });

    std::cout << "[RSU] CAM state updated: requestor_id="
        << requestorId
        << ", station_type=" << state.station_type
        << ", vehicle_role=" << state.vehicle_role
        << "\n";
}

void RoadsideUnit::handleSrem(const its::Srem& srem) {
    if (!srem.has_header()) {
        std::cerr << "[RSU] SREM ignored: missing header\n";
        return;
    }

    const uint32_t requestorId = srem.header().station_id();

    if (isDuplicate(srem)) {
        std::cout << "[RSU] Duplicate SREM ignored: requestor_id="
            << requestorId
            << ", request_id=" << srem.request().request_id()
            << "\n";
        return;
    }

    _processPriorityRequestsQueue.add(srem, srem.request().eta_seconds());

    _wsBridge.broadcastJson({
        {"type", "srem"},
        {"requestId", srem.request().request_id()},
        {"stationId", srem.requestor().station_id()},
        {"intersectionId", srem.request().intersection_id()},
        {"timestampMs", currentTimestampMs()}
        });

    std::cout << "[RSU] SREM received: requestor_id="
        << srem.requestor().station_id()
        << ", request_id=" << srem.request().request_id()
        << "\n";
}

void RoadsideUnit::startProcessingPriorityRequests() {
    _processPriorityRequestsQueue.startProcessingThread([this](const its::Srem& srem) {
        this->processPriorityRequest(srem);
    });
}

void RoadsideUnit::stopProcessingRequests() {
    _processPriorityRequestsQueue.stopProcessingThread();
}

void RoadsideUnit::processPriorityRequest(const its::Srem& srem) {
    std::cout << "[RSU] Processing SREM: requestor_id=" << srem.requestor().station_id()
        << ", intersection_id=" << srem.request().intersection_id()
        << ", request_id=" << srem.request().request_id() << "\n";

    bool priorityGranted = decidePriority(srem);
    std::cout << "Priority granted=" << std::boolalpha << priorityGranted << "\n";

    std::thread( [this, srem, priorityGranted]() {
        // Simulate some processing time
		auto sleepSeconds = rand() % 2 + 1; // Random sleep between 1-3 seconds
        std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));
        this->sendSsem(srem, priorityGranted ? its::RequestStatus::REQUEST_STATUS_GRANTED : its::RequestStatus::REQUEST_STATUS_REJECTED);
    }).detach();
}

bool RoadsideUnit::decidePriority(const its::Srem& srem) {
    //todo: [improvements] decide based on current intersection status, and if it's busy, return deny or pending

    if (!srem.requestor().role() == its::VEHICLE_ROLE_EMERGENCY &&
        !srem.requestor().role() == its::VEHICLE_ROLE_PUBLIC_TRANSPORT) {
		return false; // only emergency and public transport get priority for now
    }

    SpawnLocation currentLocation{ _state.latitude, _state.longitude };

    std::vector<VehicleState> otherVehiclesAround;
    for (const auto& [id, state] : _vehicleStates) {
        if (id == srem.header().station_id()) {
            continue; // skip the requesting vehicle
		}
        // Check if the vehicle is within the intersection area
        SpawnLocation vehicleLocation{ state.latitude, state.longitude };
        if (distanceBetween(currentLocation, vehicleLocation) < 100) {
            otherVehiclesAround.push_back(state);
        }
    }

    if (otherVehiclesAround.empty()) {
        _requestsDecisionHistory[srem.header().station_id()] = its::RequestStatus::REQUEST_STATUS_GRANTED;
        return true; // no other vehicles around, grant priority
	}

    std::vector<VehicleState> vehiclesAroundWithGrantedPriority;
    for (const auto& state : otherVehiclesAround) {
        auto requestIt = _requestsDecisionHistory.find(state.station_id);
        if (requestIt != _requestsDecisionHistory.end() &&
            requestIt->second == its::REQUEST_STATUS_GRANTED) {
            vehiclesAroundWithGrantedPriority.push_back(state);
        }
    }
    if (vehiclesAroundWithGrantedPriority.empty()) {
        _requestsDecisionHistory[srem.header().station_id()] = its::RequestStatus::REQUEST_STATUS_GRANTED;
        return true; // no other vehicles around with granted priority, grant priority
	}

    _wsBridge.broadcastJson({
        {"type", "logs"},
        {"stationId", _id},
        {"stationType", "RSU"},
        {"message", "decline priority requests because other vehicle got priority"},
        {"timestampMs", currentTimestampMs()}
        });

	_requestsDecisionHistory[srem.header().station_id()] = its::RequestStatus::REQUEST_STATUS_REJECTED;

    return false;
}

void RoadsideUnit::sendSsem(const its::Srem& srem, its::RequestStatus status) {
    const auto topic = std::format("its/rsu/{}/ssem/{}", _id, srem.header().station_id());

    its::Ssem ssem;

    auto* header = ssem.mutable_header();
    header->set_protocol_version(1);
    header->set_message_type(its::MESSAGE_TYPE_SSEM);

    header->set_station_id(_id);

    ssem.set_intersection_id(srem.request().intersection_id());
    ssem.set_request_id(srem.request().request_id());
    ssem.set_requestor_station_id(srem.header().station_id());
    ssem.set_status(status);

    std::string payload;

    if (!ssem.SerializeToString(&payload)) {
        std::cerr << "[RSU] Failed to serialize SSEM\n";
        return;
    }

    try {
		send(topic, payload);

        // todo: move to vehicle when received
        _wsBridge.broadcastJson({
            {"type", "ssem"},
            {"requestId", srem.request().request_id()},
            {"stationId", srem.requestor().station_id()},
            {"intersectionId", srem.request().intersection_id()},
			{"status", ItsEnumValueToString(status)},
            {"timestampMs", currentTimestampMs()}
            });

        std::cout << "[RSU] SSEM sent: topic=" << topic
            << ", requestor_id=" << srem.header().station_id()
            << ", request_id=" << srem.request().request_id()
            << ", status=" << ItsEnumValueToString(status)
            << "\n";
    }
    catch (const mqtt::exception& ex) {
        std::cerr << "[RSU] Failed to publish SSEM: "
            << ex.what() << "\n";
    }
}

bool RoadsideUnit::isDuplicate(const its::Srem& srem) {
    const std::string key = makeDedupKey(srem);

    if (_recentMessages.contains(key)) {
        return true;
    }

    _recentMessages[key] = std::chrono::steady_clock::now();
    return false;
}

std::string RoadsideUnit::makeDedupKey(const its::Srem& srem) const {
    return std::to_string(srem.header().station_id()) + ":" +
        std::to_string(srem.header().message_type()) + ":" +
        std::to_string(srem.generation_delta_time()) + ":" +
        std::to_string(srem.request().request_id());
}

void RoadsideUnit::cleanupOldDedupEntries() {
    const auto now = std::chrono::steady_clock::now();

    for (auto it = _recentMessages.begin(); it != _recentMessages.end(); ) {
        const auto age = now - it->second;

		if (std::chrono::duration_cast<std::chrono::milliseconds>(age).count() > MAX_GENERATION_DELTA_TIME_MS) {
            it = _recentMessages.erase(it);
        }
        else {
            ++it;
        }
    }
}

void RoadsideUnit::sendStateUpdate() {
    const auto topic = std::format("its/rsu/{}/spatem", _id);

    its::Spatem spatem;

    auto* header = spatem.mutable_header();
    header->set_protocol_version(1);
    header->set_message_type(its::MESSAGE_TYPE_SPATEM);
    header->set_station_id(_id);

    spatem.set_generation_delta_time(currentGenerationDeltaTime());

    auto* position = spatem.mutable_reference_position();
    position->set_latitude(_state.latitude);
    position->set_longitude(_state.longitude);

    spatem.set_remaining_seconds(_state.remaining_seconds);
    spatem.set_trafficlight_phase(_state.trafficlight_phase);

    std::string payload;

    if (!spatem.SerializeToString(&payload)) {
        std::cerr << "[RSU] Failed to serialize SPATEM\n";
        return;
    }

    try {
        send(topic, payload);

        _wsBridge.broadcastJson({
            {"type", "spatem"},
            {"intersectionId", _id},
            {"timestampMs", spatem.generation_delta_time()},
            {"phase", ItsEnumValueToString(spatem.trafficlight_phase())},
            {"remainingSeconds", spatem.remaining_seconds()},
            {"lat", spatem.reference_position().latitude()},
            {"lon", spatem.reference_position().longitude()}
        });

        std::cout << "[RSU] SPATEM sent: topic=" << topic
            << ", intersection_id=" << _id
            << "\n";
    }
    catch (const mqtt::exception& ex) {
        std::cerr << "[RSU] Failed to publish SPATEM: "
            << ex.what() << "\n";
    }
}

void RoadsideUnit::updateTrafficLightPhase() {
    const auto phases = std::vector{its::TRAFFICLIGHT_PHASE_RED, its::TRAFFICLIGHT_PHASE_YELLOW, its::TRAFFICLIGHT_PHASE_GREEN, its::TRAFFICLIGHT_PHASE_YELLOW, };
    const auto phasesChangeTime = std::vector{5, 2, 5, 2}; // seconds

    if (_state.remaining_seconds <= 0) {
		auto currentPhaseId = std::distance(phases.begin(), std::find(phases.begin(), phases.end(), _state.trafficlight_phase));
		auto nextPhaseId = currentPhaseId + 1;
        if (nextPhaseId == phases.size()) {
            nextPhaseId = 0;
		}
        _state.trafficlight_phase = phases[nextPhaseId];
        _state.remaining_seconds = phasesChangeTime[nextPhaseId];
	}
    else {
        _state.remaining_seconds--;
    }
}