#ifndef BATTERY_H_
#define BATTERY_H_

#include <optional>

// Returns the battery percentage (0-100), or nullopt if unavailable (always
// the case outside of PLATFORM_MIYOO_MINI builds).
std::optional<int> get_battery_percent();

#endif
