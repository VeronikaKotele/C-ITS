#include "utils.h"
#include <chrono>

uint16_t currentGenerationDeltaTime() {
    using namespace std::chrono;

    const auto now = system_clock::now();
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count();

    return static_cast<uint16_t>(ms % 65536);
}