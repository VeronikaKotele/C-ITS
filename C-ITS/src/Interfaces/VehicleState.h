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

    bool emergency_right_of_way_requested{};
    bool emergency_free_crossing_requested{};

    uint32_t last_generation_delta_time{};
    std::chrono::steady_clock::time_point last_seen{};
};