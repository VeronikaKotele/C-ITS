#pragma once

#include <cstdint>
#include <Interfaces/SpawnLocation.h>

uint64_t currentTimestampMs();
uint16_t currentGenerationDeltaTime();

double calculateHeading(SpawnLocation current, SpawnLocation target);
double distanceBetween(const SpawnLocation& a, const SpawnLocation& b);
SpawnLocation moveStep(const SpawnLocation& current, double heading_deg, double distance_m);