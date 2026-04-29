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
#include "spatem.pb.h"
#include "InterfacesTranslator.h"

Vehicle::Vehicle(VehicleType type, VehicleState state, WebSocketBridge& wsBridge)
    : MqttClient(state.station_id),
    _vehicleType(type),
    _vehicleRole(ItsVehicleRoleFromVehicleType(type)),
    _stationType(ItsStationTypeFromVehicleType(type)),
    _state(state),
	_wsBridge(wsBridge),
    _callback(*this)
{
    _client.set_callback(_callback);
	_state.generation_delta_time = currentGenerationDeltaTime();
	_state.station_type = _stationType;
	_state.vehicle_role = _vehicleRole;
	_basicSpeed = _state.speed;
}

std::string Vehicle::name() const {
    return "{}" , _vehicleType + " no {}" , std::to_string(_id);
}

std::string Vehicle::to_json() const {
    return R"({"vehicle_id":{}})" + std::to_string(_id);
}

void Vehicle::setPath(std::vector<SpawnLocation> locations) {
    setDestination(locations.front());
    locations.erase(locations.begin());
    _path = std::move(locations);
}

void Vehicle::setDestination(SpawnLocation targetLocation) {
	_destination = targetLocation;

    SpawnLocation currentLocation{ _state.latitude, _state.longitude };
    _state.heading = calculateHeading(currentLocation, targetLocation);
}

void Vehicle::changeSpeed(double deltaKmH) {
    _state.speed += deltaKmH;
    if (_state.speed < 0) {
        _state.speed = 0;
	}
}

void Vehicle::move() {
    if (_state.speed <= 0) {
        // Vehicle is stopped, not moving
        return;
	}

	SpawnLocation currentLocation{ _state.latitude, _state.longitude };
    if (distanceBetween(currentLocation, _destination) < 20) { // if within 50 meter of destination
		log("Reached destination, redirect.");
        if (_path.empty()) {
            log("No more destinations in path, stopping.");
            _state.speed = 0;
            return;
        }
        setDestination(_path.front());
        _path.erase(_path.begin());
	}

    try {
        double deltaTime = (currentGenerationDeltaTime() - _state.generation_delta_time) / 1000.0; // convert ms to seconds
        if (deltaTime < 0) {
			deltaTime += MAX_GENERATION_DELTA_TIME_MS / 1000.0; // handle wrap-around
		}
		deltaTime = std::clamp(deltaTime, 0.0, 2.0); // max 2 seconds to prevent unrealistic jumps
        double speed_mps = _state.speed / 3.6; // from km/h to m/s
        double distancePassed = speed_mps * deltaTime; // meters

        auto newLocation = moveStep(currentLocation, _state.heading, distancePassed);
        _state.latitude = newLocation.latitude;
        _state.longitude = newLocation.longitude;
        _state.generation_delta_time = currentGenerationDeltaTime();
    }
    catch (const std::exception& ex) {
        log(std::format("Failed to move: {}", ex.what()));
    }
}

void Vehicle::listenToRsuState() {
    subscribe("its/rsu/+/spatem");
}

void Vehicle::sendCurrentState() {
	const auto topic = std::format("its/vehicle/{}/cam", _id);

    its::Cam cam;

    auto* header = cam.mutable_header();
    header->set_protocol_version(1);
    header->set_message_type(its::MESSAGE_TYPE_CAM);
    header->set_station_id(_id);

    auto* ccam = cam.mutable_cam();
	ccam->set_generation_delta_time(_state.generation_delta_time);
    
	auto* basicCam = ccam->mutable_basic_container();
    basicCam->set_station_type(this->_stationType);

	auto* position = basicCam->mutable_reference_position();
    position->set_latitude(_state.latitude);
    position->set_longitude(_state.longitude);

	auto* highFrequencyCam = ccam->mutable_high_frequency_container();
	auto* highFrequencyBasicCam = highFrequencyCam->mutable_basic_vehicle_container_high_frequency();
	highFrequencyBasicCam->set_speed(_state.speed);
	highFrequencyBasicCam->set_heading(_state.heading);

    auto* lowFrequencyCam = ccam->mutable_low_frequency_container();
    auto* lowFrequencyBasicVehicleCam = lowFrequencyCam->mutable_basic_vehicle_container_low_frequency();
    lowFrequencyBasicVehicleCam->set_vehicle_role(this->_vehicleRole);

    if (this->_vehicleRole == its::VEHICLE_ROLE_EMERGENCY) {
        auto* specialVehicleCam = ccam->mutable_special_vehicle_container();
        auto* emergencyCam = specialVehicleCam->mutable_emergency_container();
        auto* light_bar_siren_in_use = emergencyCam->mutable_light_bar_siren_in_use();
        light_bar_siren_in_use->set_light_bar_activated(true);
        light_bar_siren_in_use->set_siren_activated(false);
    }

    std::string payload;
    cam.SerializeToString(&payload);

    try {
        log(std::format("Sending CAM: generation_delta_time = {}", ccam->generation_delta_time()));

        send(topic, payload);
    }
    catch (const mqtt::exception& ex) {
        log(std::format("Failed to publish CAM: generation_delta_time = {}, error = {}", ccam->generation_delta_time(), ex.what()));
    }
}

void Vehicle::requestPriority() {
    if (_roadsideUnits.empty()) {
        return;
	}

    const SpawnLocation currentPosition{ _state.latitude, _state.longitude };

    for (const auto& [rsuId, rsuState] : _roadsideUnits) {
		auto prevrequestIt = _rsuPriorityRequests.find(rsuId);
        if (prevrequestIt != _rsuPriorityRequests.end() &&
            prevrequestIt->second._last_request_status == its::RequestStatus::REQUEST_STATUS_PENDING) {
            log(std::format("Already requested priority for RSU {} and it's still pending, skipping new request\n", rsuId));
            continue;
		}

        auto rsuPosition = SpawnLocation{ rsuState.latitude, rsuState.longitude };
		auto trafficlight_phase = rsuState.trafficlight_phase;
		auto remainingTime = rsuState.remaining_seconds;
		auto timeToReach = timeToReachSec(currentPosition, rsuPosition, _state.speed);
		auto distance = distanceBetween(currentPosition, rsuPosition);

		if (distance > 500.0) { // if RSU is more than 500 meters away, do not request priority
            log(std::format("RSU {} is too far: {} m\n", rsuId, distance));
			continue;
		}
        log(std::format("Approaching RSU: station_id = {}, distance = {}, time to reach = {}", rsuId, distance, timeToReach));

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
        position->set_latitude(_state.latitude);
        position->set_longitude(_state.longitude);

        auto* request = srem.mutable_request();
        request->set_request_id(rand() % 100);
        request->set_intersection_id(rsuId);
        request->set_eta_seconds(timeToReach);

        std::string payload;
        srem.SerializeToString(&payload);

        try {
            log(std::format("Sending SREM: generation_delta_time = {}, request_id = {}", srem.generation_delta_time(), request->request_id()));

            send(topic, payload);
        }
        catch (const mqtt::exception& ex) {
            log(std::format("Failed to publish SREM: generation_delta_time = {}, request_id = {}, error = {}", srem.generation_delta_time(), request->request_id(), ex.what()));
            return;
        }

        _rsuPriorityRequests[request->request_id()] = {
            ._last_request_id = request->request_id(),
            ._last_request_status = its::RequestStatus::REQUEST_STATUS_PENDING,
			._last_request_timestamp_ms = currentGenerationDeltaTime()
		};
    }
}

void Vehicle::listenToPriorityResponce() {
    subscribe(std::format( "its/rsu/+/ssem/{}", _id));
}

Vehicle::Callback::Callback(Vehicle& owner) : _owner(owner) {}

void Vehicle::Callback::message_arrived(mqtt::const_message_ptr msg) {
    const std::string topic = msg->get_topic();
    const std::string payload = msg->to_string();

    _owner.log(std::format("message arrived : {}", topic));

    try {
        if (topic.find("/ssem") != std::string::npos) {
            its::Ssem ssem;

            if (!ssem.ParseFromString(payload)) {
                _owner.log(std::format("Failed to parse SSEM from topic: {}", topic));
                return;
            }

            _owner.handlePriorityResponce(ssem);
            return;
        }
        else if (topic.find("/spatem") != std::string::npos) {
            its::Spatem spatem;
            if (!spatem.ParseFromString(payload)) {
                _owner.log(std::format("Failed to parse SPATEM from topic: {}", topic));
                return;
            }

            _owner.handleRsuStateUpdate(spatem);
            return;
		}

        _owner.log(std::format("Ignored unsupported topic: {}", topic));
    }
    catch (const std::exception& ex) {
        _owner.log(std::format("Exception in message_arrived: {}", ex.what()));
    }
}

void Vehicle::handleRsuStateUpdate(const its::Spatem& spatem) {
	auto rsuId = spatem.header().station_id();

    log(std::format("received SPATEM: intersection_id = {}, current_light_state = {}, remaining_seconds = {}", rsuId, ItsEnumValueToString(spatem.trafficlight_phase()), spatem.remaining_seconds()));

	// Update internal state of approaching RSUs
	auto stateIt = _roadsideUnits.find(rsuId);
    if (stateIt == _roadsideUnits.end())
    {
        _roadsideUnits.emplace(rsuId, RsuState{
            .station_id = rsuId,
            .latitude = spatem.reference_position().latitude(),
            .longitude = spatem.reference_position().longitude(),
            .trafficlight_phase = spatem.trafficlight_phase(),
            .remaining_seconds = static_cast<int>(spatem.remaining_seconds()),
            .generation_delta_time = currentGenerationDeltaTime()
            });
    }
    else {
        auto& rsuState = stateIt->second;
        rsuState.latitude = spatem.reference_position().latitude();
        rsuState.longitude = spatem.reference_position().longitude();
        rsuState.trafficlight_phase = spatem.trafficlight_phase();
        rsuState.remaining_seconds = static_cast<int>(spatem.remaining_seconds());
		rsuState.generation_delta_time = currentGenerationDeltaTime();
    }
}

void Vehicle::handlePriorityResponce(const its::Ssem& ssem) {
    try {
		auto requestor_id = ssem.requestor_station_id();
		auto request_id = ssem.request_id();
		auto status = ssem.status();
		auto rsuId = ssem.intersection_id();

		auto requestInfoIt = _rsuPriorityRequests.find(rsuId);
        if (requestInfoIt == _rsuPriorityRequests.end()) {
            log(std::format("Received SSEM for resolved request: {}", request_id));
            return;
		}
        auto& requestState = requestInfoIt->second;

		if (requestor_id == _id && request_id == requestState._last_request_id) {

            log(std::format("received SSEM: requestor_id = {}, request_id = {}, status = {}", requestor_id, request_id, ItsEnumValueToString(status)));

            requestState._last_request_status = status;
			requestState._last_request_timestamp_ms = currentGenerationDeltaTime();

            log(std::format("priority request {} status {}", request_id, ItsEnumValueToString(status)));
            reactOnPriorityResponce(ssem.status() == its::RequestStatus::REQUEST_STATUS_GRANTED);
        }
        else {
			log(std::format("Ignored SSEM for other request: requestor_id = {} (should be {})", requestor_id, _id));

        }
    }
    catch (const std::exception& ex) {
        log(std::format("Exception in message_arrived: {}", ex.what()));
    }
}


void Vehicle::reactOnPriorityResponce(bool granted) {
    if (granted) {
		_state.speed = _basicSpeed; // restore to basic speed when granted priority
    }
    else {
        _state.speed = 0;
	}
}

void Vehicle::log(std::string message) const {
    std::cout << "[" << name() << "] " << message << "\n";

    _wsBridge.broadcastJson({
        {"type", "logs"},
        {"stationId", _id},
        {"stationType", ItsEnumValueToString(_stationType)},
        {"message", message},
        {"timestampMs", currentTimestampMs()}
        });
}