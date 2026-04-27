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
#include "SendMessageQueue.h"
#include "ProcessMessageQueue.h"

class RoadsideUnit : public MqttClient {
public:
    RoadsideUnit(uint32_t id);

	void subscribeToListenCam();
    void subscribeToListenSrem();
    void startProcessingPriorityRequests();
    void stopProcessingRequests();

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

	bool decidePriority(const its::Srem& srem);

    Callback _callback;

    std::unordered_map<std::string, std::chrono::steady_clock::time_point> _recentMessages;
    std::unordered_map<uint32_t, VehicleState> _vehicleStates;
    ProcessMessageQueue<its::Srem> _processPriorityRequestsQueue;
};