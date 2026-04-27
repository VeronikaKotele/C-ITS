#pragma once

#include "MqttClient.h"
#include "etsi_common.pb.h"
#include "VehicleTypes.h"

class Vehicle : public MqttClient {
public:
    Vehicle(uint32_t id, VehicleType type);

    std::string to_json() const;

    void sendSpeedStatus();

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

	Callback _callback;
    VehicleType _vehicleType;
    its::StationType _stationType;
    its::VehicleRole _vehicleRole;

	static its::StationType deductItsStationType(VehicleType type);
    static its::VehicleRole deductItsVehicleRole(VehicleType type);
};
