#pragma once
#include "Interfaces/VehicleTypes.h"
#include "etsi_common.pb.h"
#include "ssem.pb.h"

its::StationType ItsStationTypeFromVehicleType(VehicleType type);
its::VehicleRole ItsVehicleRoleFromVehicleType(VehicleType type);

template<typename ItsType>
std::string ItsEnumValueToString(const ItsType& requestStatus, bool full = false) {
	auto fullName = google::protobuf::GetEnumDescriptor<ItsType>()->FindValueByNumber(requestStatus)->name();
	if (full) {
		return fullName;
	}

	// SHort name is the part after the second underscore, e.g. for STATION_TYPE_PUBLIC_TRANSPORT it would be PUBLIC_TRANSPORT
	auto firstUnderscorePos = fullName.find('_');
	if (firstUnderscorePos != std::string::npos) {
		auto secondUnderscorePos = fullName.find('_', firstUnderscorePos + 1);
		if (secondUnderscorePos != std::string::npos) {
			return fullName.substr(secondUnderscorePos + 1);
		}
	}
	return fullName;
}