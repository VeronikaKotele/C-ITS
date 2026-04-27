#include "RoadsideUnit.h"
#include <mqtt/async_client.h>
#include <iostream>
#include "constants.h"
#include "etsi_common.pb.h"
#include "cam.pb.h"
#include "srem.pb.h"
#include "ssem.pb.h"

RoadsideUnit::RoadsideUnit(uint32_t id) : MqttClient(id), _callback(*this){
    _client.set_callback(_callback);
}

void RoadsideUnit::subscribeToListenCam() {
    subscribe("its/vehicle/+/cam");
}

void RoadsideUnit::subscribeToListenSrem() {
    subscribe("its/vehicle/+/srem");
}

RoadsideUnit::Callback::Callback(RoadsideUnit& owner) : _owner(owner) {}

void RoadsideUnit::Callback::message_arrived(mqtt::const_message_ptr msg) {
    const std::string topic = msg->get_topic();
    const std::string payload = msg->to_string();

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

    const uint32_t stationId = header.station_id();

    if (!params.has_basic_container()) {
        std::cerr << "[RSU] CAM ignored: missing basic container\n";
        return;
    }

    const auto& basic = params.basic_container();

    VehicleState state{};
    state.station_id = stationId;
    state.station_type = basic.station_type();
    state.last_generation_delta_time = params.generation_delta_time();
    state.last_seen = std::chrono::steady_clock::now();

    if (basic.has_reference_position()) {
        state.latitude = basic.reference_position().latitude();
        state.longitude = basic.reference_position().longitude();
    }

    state.vehicle_role = its::VEHICLE_ROLE_DEFAULT;
    state.emergency_free_crossing_requested = false;
    state.emergency_right_of_way_requested = false;

    if (params.has_low_frequency_container() &&
        params.low_frequency_container().has_basic_vehicle_container_low_frequency()) {
        state.vehicle_role =
            params.low_frequency_container()
            .basic_vehicle_container_low_frequency()
            .vehicle_role();
    }

    if (params.has_special_vehicle_container()) {
        const auto& special = params.special_vehicle_container();

        if (special.has_emergency_container()) {
            const auto& emergency = special.emergency_container();

            state.vehicle_role = its::VEHICLE_ROLE_EMERGENCY;

            if (emergency.has_emergency_priority()) {
                state.emergency_right_of_way_requested =
                    emergency.emergency_priority().request_for_right_of_way();

                state.emergency_free_crossing_requested =
                    emergency.emergency_priority()
                    .request_for_free_crossing_at_a_traffic_light();
            }
        }

        if (special.has_public_transport_container()) {
            state.vehicle_role = its::VEHICLE_ROLE_PUBLIC_TRANSPORT;
        }
    }

    _vehicleStates[stationId] = state;

    std::cout << "[RSU] CAM state updated: station_id="
        << stationId
        << ", station_type=" << state.station_type
        << ", vehicle_role=" << state.vehicle_role
        << "\n";
}

void RoadsideUnit::handleSrem(const its::Srem& srem) {
    if (!srem.has_header()) {
        std::cerr << "[RSU] SREM ignored: missing header\n";
        return;
    }

    const uint32_t stationId = srem.header().station_id();

    if (isDuplicate(srem)) {
        std::cout << "[RSU] Duplicate SREM ignored: station_id="
            << stationId
            << ", request_id=" << srem.request().request_id()
            << "\n";
        return;
    }

    enum PriorityDecision {
        PRIORITY_DECISION_NONE,
        PRIORITY_DECISION_MEDIUM,
        PRIORITY_DECISION_HIGH
	};

    its::RequestStatus status = its::RequestStatus::REQUEST_STATUS_UNKNOWN;

    if (srem.requestor().role() == its::VEHICLE_ROLE_EMERGENCY ||
        srem.requestor().role() == its::VEHICLE_ROLE_PUBLIC_TRANSPORT) {
        //todo: [improvements] have current intersection status, and if it's busy, deny or pendings
        status = its::RequestStatus::REQUEST_STATUS_GRANTED;
    }
    else {
        status = its::RequestStatus::REQUEST_STATUS_REJECTED;
    }

    const bool granted =
        status == its::RequestStatus::REQUEST_STATUS_GRANTED;

    std::cout << "[RSU] SREM processed: station_id=" << stationId
        << ", intersection_id=" << srem.request().intersection_id()
        << ", request_id=" << srem.request().request_id()
        << ", granted=" << std::boolalpha << granted
        << "\n";

	//todo: [performance] processing SREM and sending SSEM in a separate thread pool, to avoid blocking MQTT callback thread
    //   if (granted) {
    //       _priorityRequestsQueue.emplace(stationId, srem.request().eta_seconds());
	//}
	sendSsem(srem, status);
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

        std::cout << "[RSU] SSEM sent: topic="
            << topic
            << ", requestor_station_id=" << srem.header().station_id()
            << ", request_id=" << srem.request().request_id()
            << ", granted=" << std::boolalpha << (status == its::RequestStatus::REQUEST_STATUS_GRANTED)
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