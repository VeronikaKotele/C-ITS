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
#include "WebSocketBridge.h"
#include "Interfaces/VehicleState.h"
#include "Interfaces/RsuState.h"
#include "Interfaces/SpawnLocation.h"

class RoadsideUnit : public MqttClient {
public:
    RoadsideUnit(uint32_t id, SpawnLocation location, WebSocketBridge& wsBridge);

	void listenVehiclesUpdate();
    void listenPriorityRequests();
    void startProcessingPriorityRequests();
    void stopProcessingRequests();
	void sendStateUpdate();

	void updateTrafficLightPhase();

private:
    class Callback : public virtual mqtt::callback {
    public:
        explicit Callback(RoadsideUnit& owner);

        void message_arrived(mqtt::const_message_ptr msg) override;

    private:
        RoadsideUnit& _owner;
    };

    void handleCam(const its::Cam& cam);
    void handleSrem(const its::Srem& srem);
	void sendSsem(const its::Srem& srem, its::RequestStatus status);

    bool isDuplicate(const its::Srem& srem);
    std::string makeDedupKey(const its::Srem& srem) const;
    void cleanupOldDedupEntries();

    void processPriorityRequest(const its::Srem& srem);
	bool decidePriority(const its::Srem& srem);

    Callback _callback;

    RsuState _state;

    std::unordered_map<std::string, std::chrono::steady_clock::time_point> _recentMessages;
    std::unordered_map<uint32_t, VehicleState> _vehicleStates;
    ProcessMessageQueue<its::Srem> _processPriorityRequestsQueue;
    WebSocketBridge& _wsBridge;
};