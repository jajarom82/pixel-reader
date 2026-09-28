#include "./battery.h"

#include <fstream>

std::optional<int> get_battery_percent()
{
#if PLATFORM_MIYOO_MINI
    // Both OnionOS and MiniUI on Miyoo Mini/Mini+ run a background daemon
    // that keeps this file updated with the battery percentage - reading
    // the PMIC directly requires I2C access and differs between the
    // original Mini and the Mini+, so this is the one path that works
    // across both without extra permissions. If a given firmware build
    // uses a different path, this is the only place that needs to change
    // (see src/sandbox: `sandbox battery` prints what this returns).
    constexpr const char *BATTERY_PATH = "/tmp/battery";

    std::ifstream file(BATTERY_PATH);
    if (!file)
    {
        return std::nullopt;
    }

    int percent = -1;
    file >> percent;
    if (!file || percent < 0 || percent > 100)
    {
        return std::nullopt;
    }

    return percent;
#else
    return std::nullopt;
#endif
}
