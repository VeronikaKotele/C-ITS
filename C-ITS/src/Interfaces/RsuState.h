#pragma once

#include <numeric>
#include "etsi_common.pb.h"
#include "spatem.pb.h"

struct RsuState {
    uint32_t station_id{};
    double latitude{};
    double longitude{};

    its::TrafficLightPhase traffic_light_phase{ its::TrafficLightPhase::TRAFFIC_LIGHT_PHASE_RED };
	int remaining_seconds{};

    uint32_t generation_delta_time{};
};