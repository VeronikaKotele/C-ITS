#pragma once

#include <ostream>
#include <string>
#include <string_view>

enum class VehicleType
{
	UNKNOWN,
	CAR,
	BUS,
	TRUCK,
	EMERGENCY,
	BICYCLE
};

inline std::ostream& operator<<(std::ostream& os, VehicleType vt)
{
	switch (vt)
	{
	case VehicleType::CAR:        os << "CAR";        break;
	case VehicleType::BUS:        os << "BUS";        break;
	case VehicleType::TRUCK:      os << "TRUCK";      break;
	case VehicleType::EMERGENCY:  os << "EMERGENCY";  break;
	case VehicleType::BICYCLE:    os << "BICYCLE";    break;
	case VehicleType::UNKNOWN:
	default:                      os << "UNKNOWN";    break;
	}
	return os;
}

// Return a std::string (heap-allocated) representation.
inline std::string to_string(VehicleType vt)
{
	switch (vt)
	{
	case VehicleType::CAR:        return "CAR";
	case VehicleType::BUS:        return "BUS";
	case VehicleType::TRUCK:      return "TRUCK";
	case VehicleType::EMERGENCY:  return "EMERGENCY";
	case VehicleType::BICYCLE:    return "BICYCLE";
	case VehicleType::UNKNOWN:
	default:                      return "UNKNOWN";
	}
}

// Return a constexpr-friendly view to a literal; useful when no allocation is needed.
inline std::string_view to_string_view(VehicleType vt) noexcept
{
	switch (vt)
	{
	case VehicleType::CAR:        return "CAR";
	case VehicleType::BUS:        return "BUS";
	case VehicleType::TRUCK:      return "TRUCK";
	case VehicleType::EMERGENCY:  return "EMERGENCY";
	case VehicleType::BICYCLE:    return "BICYCLE";
	case VehicleType::UNKNOWN:
	default:                      return "UNKNOWN";
	}
}

// Convenience operator+ overloads so concatenation with std::string / const char* works.
inline std::string operator+(const std::string& lhs, VehicleType vt)
{
	return lhs + to_string(vt);
}

inline std::string operator+(VehicleType vt, const std::string& rhs)
{
	return to_string(vt) + rhs;
}

inline std::string operator+(const char* lhs, VehicleType vt)
{
	return std::string(lhs) + to_string(vt);
}

inline std::string operator+(VehicleType vt, const char* rhs)
{
	return to_string(vt) + rhs;
}

