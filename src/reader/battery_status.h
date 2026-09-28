#ifndef BATTERY_STATUS_H_
#define BATTERY_STATUS_H_

#include "util/timer.h"

#include <optional>

// Polls the platform battery percentage on a coarse timer (BATTERY_POLL_
// INTERVAL_SEC) and caches the last-known value, so callers don't hit the
// underlying (file) read every render.
class BatteryStatus
{
    std::optional<int> percent;
    Timer poll_timer;
    bool has_polled = false;

public:
    // Call once per main loop iteration. Cheap when not yet due to
    // re-check. Returns true if the percentage changed as a result of this
    // call (i.e. a redraw of the indicator is warranted).
    bool poll();

    std::optional<int> get_percent() const;
};

#endif
