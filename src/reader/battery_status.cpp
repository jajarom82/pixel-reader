#include "./battery_status.h"

#include "./config.h"
#include "sys/battery.h"

bool BatteryStatus::poll()
{
    if (has_polled && poll_timer.elapsed_sec() < BATTERY_POLL_INTERVAL_SEC)
    {
        return false;
    }
    has_polled = true;
    poll_timer.reset();

    auto new_percent = get_battery_percent();
    if (new_percent != percent)
    {
        percent = new_percent;
        return true;
    }

    return false;
}

std::optional<int> BatteryStatus::get_percent() const
{
    return percent;
}
