#pragma once
#include "VehicleTypes.h"
#include "etsi_common.pb.h"
#include "ssem.pb.h"

its::StationType ItsStationTypeFromVehicleType(VehicleType type);
its::VehicleRole ItsVehicleRoleFromVehicleType(VehicleType type);

std::string ItsEnumValueToString(const its::RequestStatus& requestStatus);