#include <iostream>

#include "sys/battery.h"

void battery_probe()
{
    auto percent = get_battery_percent();
    if (percent)
    {
        std::cout << "battery: " << *percent << "%" << std::endl;
    }
    else
    {
        std::cout << "battery: unavailable" << std::endl;
    }
}
