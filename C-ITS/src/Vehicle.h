#pragma once

#include "MqttClient.h"
#include "etsi_common.pb.h"
#include "ssem.pb.h"
#include "WebSocketBridge.h"
#include "Interfaces/VehicleTypes.h"
#include "Interfaces/SpawnLocation.h"
#include "Interfaces/VehicleState.h"
#include "Interfaces/RsuState.h"

class Vehicle : public MqttClient {
public:
    Vehicle(VehicleType type, VehicleState state, WebSocketBridge& wsBridge);

    std::string name() const;

    std::string to_json() const;
    void setPath(std::vector<SpawnLocation> locations);
    void move();
	void changeSpeed(double deltaKmH);
    void sendCurrentState();
	void listenToRsuState();

    void requestPriority();
    void listenToPriorityResponce();

private:
    class Callback : public virtual mqtt::callback {
    public:
        explicit Callback(Vehicle& owner);

        void message_arrived(mqtt::const_message_ptr msg) override;

    private:
        Vehicle& _owner;
    };

    void setDestination(SpawnLocation location);
	void handleRsuStateUpdate(const its::Spatem& spatem);
    void handlePriorityResponce(const its::Ssem& ssem);
	void reactOnPriorityResponce(bool granted);

    void log(std::string message) const;

    WebSocketBridge& _wsBridge;

	Callback _callback;
    VehicleType _vehicleType;
    its::StationType _stationType;
    its::VehicleRole _vehicleRole;

    VehicleState _state;
    SpawnLocation _destination{ 0, 0 };
	std::vector<SpawnLocation> _path;
	double _basicSpeed = 0;

    struct PriorityRequestInfo {
        uint32_t _last_request_id;
        its::RequestStatus _last_request_status;
		uint32_t _last_request_timestamp_ms;
	};

	std::map<uint32_t, PriorityRequestInfo> _rsuPriorityRequests; // key is rsu station_id

    std::map<uint32_t, RsuState> _roadsideUnits; // key is rsu station_id
};
