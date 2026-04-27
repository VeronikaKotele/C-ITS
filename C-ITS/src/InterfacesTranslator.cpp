#include "InterfacesTranslator.h"

its::StationType deductItsStationType(VehicleType type) {
    switch (type)
    {
    case VehicleType::UNKNOWN:
        return its::StationType::STATION_TYPE_UNKNOWN;
    case VehicleType::CAR:
        return its::StationType::STATION_TYPE_PASSENGER_CAR;
    case VehicleType::BUS:
        return its::StationType::STATION_TYPE_BUS;
    case VehicleType::TRUCK:
        return its::StationType::STATION_TYPE_HEAVY_TRUCK;
    case VehicleType::EMERGENCY:
        return its::StationType::STATION_TYPE_SPECIAL_VEHICLES;
    case VehicleType::BICYCLE:
        return its::StationType::STATION_TYPE_CYCLIST;
    default:
        return its::StationType::STATION_TYPE_UNKNOWN;
    }
}

its::VehicleRole deductItsVehicleRole(VehicleType type) {
    switch (type)
    {
    case VehicleType::UNKNOWN:
    case VehicleType::CAR:
    case VehicleType::BICYCLE:
        return its::VehicleRole::VEHICLE_ROLE_DEFAULT;
    case VehicleType::BUS:
        return its::VehicleRole::VEHICLE_ROLE_PUBLIC_TRANSPORT;
    case VehicleType::TRUCK:
        return its::VehicleRole::VEHICLE_ROLE_COMMERCIAL;
    case VehicleType::EMERGENCY:
        return its::VehicleRole::VEHICLE_ROLE_EMERGENCY;
    default:
        return its::VehicleRole::VEHICLE_ROLE_DEFAULT;
    }
}

std::string ItsEnumValueToString(const its::RequestStatus& requestStatus) {
    return google::protobuf::GetEnumDescriptor<its::RequestStatus>()->FindValueByNumber(requestStatus)->name();
}