#pragma once

#include <numeric>
#include "etsi_common.pb.h"

struct VehicleState {
    uint32_t station_id{};
    its::StationType station_type{ its::STATION_TYPE_UNKNOWN };
    its::VehicleRole vehicle_role{ its::VEHICLE_ROLE_DEFAULT };

    double latitude{};
    double longitude{};

	double speed{}; // in km/h
	double heading{}; // in 360 degrees, where 0 is north, 90 is east, etc.

    uint32_t generation_delta_time{}; // by ETSI EN 302 637, timestamp (ms) mod 65536
};