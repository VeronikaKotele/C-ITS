#pragma once
#include "VehicleTypes.h"
#include "etsi_common.pb.h"
#include "ssem.pb.h"

its::StationType ItsStationTypeFromVehicleType(VehicleType type);
its::VehicleRole ItsVehicleRoleFromVehicleType(VehicleType type);

template<typename ItsType>
std::string ItsEnumValueToString(const ItsType& requestStatus) {
    return google::protobuf::GetEnumDescriptor<ItsType>()->FindValueByNumber(requestStatus)->name();
}