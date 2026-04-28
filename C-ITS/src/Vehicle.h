#pragma once

#include "MqttClient.h"
#include "etsi_common.pb.h"
#include "ssem.pb.h"
#include "Interfaces/VehicleTypes.h"
#include "Interfaces/SpawnLocation.h"
#include "Interfaces/VehicleState.h"

class Vehicle : public MqttClient {
public:
    Vehicle(VehicleType type, VehicleState state);

    std::string name() const;

    std::string to_json() const;

    void setDestination(SpawnLocation location);
	void changeSpeed(double deltaKmH);
    void move();

    void sendCAM();
    void requestPriority();
	void priorityGranted(bool granted);
    void subscribeToListenSsem();

private:
    class Callback : public virtual mqtt::callback {
    public:
        explicit Callback(Vehicle& owner);

        void message_arrived(mqtt::const_message_ptr msg) override;

    private:
        Vehicle& _owner;
    };

    void handleSsem(const its::Ssem& ssem);

	Callback _callback;
    VehicleType _vehicleType;
    its::StationType _stationType;
    its::VehicleRole _vehicleRole;

    VehicleState _state;

    bool _priority_requested;
	uint32_t _last_request_id;
	its::RequestStatus _last_request_status;
};
