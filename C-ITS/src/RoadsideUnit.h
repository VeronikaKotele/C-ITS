#pragma once

#include "MqttClient.h"
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <string>
#include "etsi_common.pb.h"
#include "cam.pb.h"
#include "srem.pb.h"
#include "ssem.pb.h"

class RoadsideUnit : public MqttClient {
public:
    RoadsideUnit(uint32_t id);

	void subscribeToListenCam();
    void subscribeToListenSrem();

private:
    class Callback : public virtual mqtt::callback {
    public:
        explicit Callback(RoadsideUnit& owner);

        void message_arrived(mqtt::const_message_ptr msg) override;

    private:
        RoadsideUnit& _owner;
    };

    struct VehicleState {
        uint32_t station_id{};
        its::StationType station_type{ its::STATION_TYPE_UNKNOWN };
        its::VehicleRole vehicle_role{ its::VEHICLE_ROLE_DEFAULT };

        double latitude{};
        double longitude{};

        bool emergency_right_of_way_requested{};
        bool emergency_free_crossing_requested{};

        uint32_t last_generation_delta_time{};
        std::chrono::steady_clock::time_point last_seen{};
    };

    void handleCam(const its::Cam& cam);
    void handleSrem(const its::Srem& srem);
	void sendSsem(const its::Srem& srem, its::RequestStatus status);

    bool isDuplicate(const its::Srem& srem);
    std::string makeDedupKey(const its::Srem& srem) const;
    void cleanupOldDedupEntries();

    Callback _callback;

    std::unordered_map<std::string, std::chrono::steady_clock::time_point> _recentMessages;
    std::unordered_map<uint32_t, VehicleState> _vehicleStates;
    std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>> _priorityRequestsQueue;
};