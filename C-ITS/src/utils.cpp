#include "utils.h"
#include <chrono>
#include <cmath>

uint64_t currentTimestampMs() {
    using namespace std::chrono;

    const auto now = system_clock::now();
    return duration_cast<milliseconds>(now.time_since_epoch()).count();
}

uint16_t currentGenerationDeltaTime() {
    using namespace std::chrono;

    const auto now = system_clock::now();
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count();

    return static_cast<uint16_t>(ms % 65536);
}

double degToRad(double deg) {
    return deg * M_PI / 180.0;
}

double radToDeg(double rad) {
    return rad * 180.0 / M_PI;
}

double normalizeDegrees(double deg) {
    while (deg < 0) deg += 360;
    while (deg >= 360) deg -= 360;
    return deg;
}

double calculateHeading(SpawnLocation current, SpawnLocation target) {
    double a1 = degToRad(current.latitude);
    double a2 = degToRad(target.latitude);
    double d = degToRad(target.longitude - current.longitude);

    double y = std::sin(d) * std::cos(a2);
    double x =
        std::cos(a1) * std::sin(a2) -
        std::sin(a1) * std::cos(a2) * std::cos(d);

    double heading = std::atan2(y, x);
    if (heading < 0) {
        heading += 2 * M_PI;
	}

    return normalizeDegrees(radToDeg(heading));
}

double distanceBetween(const SpawnLocation& a, const SpawnLocation& b) {
    // Haversine formula -> returns distance in meters
    constexpr double EARTH_RADIUS_M = 6371000.0;
    double dLat = degToRad(b.latitude - a.latitude);
    double dLon = degToRad(b.longitude - a.longitude);
    double lat1 = degToRad(a.latitude);
    double lat2 = degToRad(b.latitude);

    double sin_dlat = std::sin(dLat / 2.0);
    double sin_dlon = std::sin(dLon / 2.0);
    double hav = sin_dlat * sin_dlat + std::cos(lat1) * std::cos(lat2) * sin_dlon * sin_dlon;
    double c = 2.0 * std::atan2(std::sqrt(hav), std::sqrt(1.0 - hav));
    return EARTH_RADIUS_M * c;
}

SpawnLocation moveStep(
	const SpawnLocation& current,
    double heading_deg,
    double distance_m
) {
    double φ1 = degToRad(current.latitude);
    double λ1 = degToRad(current.longitude);
    double θ = degToRad(heading_deg);

    constexpr double EARTH_RADIUS_M = 6371000.0;
    double δ = distance_m / EARTH_RADIUS_M;

    double sinφ2 =
        std::sin(φ1) * std::cos(δ) +
        std::cos(φ1) * std::sin(δ) * std::cos(θ);

    sinφ2 = std::max(-1.0, std::min(sinφ2, 1.0));

    double φ2 = std::asin(sinφ2);

    double λ2 = λ1 + std::atan2(
        std::sin(θ) * std::sin(δ) * std::cos(φ1),
        std::cos(δ) - std::sin(φ1) * std::sin(φ2)
    );

    auto newLocation = SpawnLocation{ radToDeg(φ2), radToDeg(λ2) };
	if (newLocation.latitude > 90)
        newLocation.latitude = 90;
	if (newLocation.latitude < -90)
        newLocation.latitude = -90;
	if (newLocation.longitude > 180)
        newLocation.longitude -= 360;
	if (newLocation.longitude < -180)
        newLocation.longitude += 360;
    return newLocation;
}

int timeToReachSec(SpawnLocation current, SpawnLocation target, double speedKmH) {
    double distance = distanceBetween(current, target);
    double speed_mps = speedKmH * 1000.0 / 3600.0;  // Convert speed to m/s
    return static_cast<int>(distance / speed_mps);
}
