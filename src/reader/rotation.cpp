#include "./rotation.h"

#include <utility>
#include <vector>

namespace
{

struct RotationDef
{
    std::string display_name;
    int degrees;
};

const std::vector<std::pair<std::string, RotationDef>> rotation_defs = {
    {"0",   { "0°",   0   }},
    {"90",  { "90°",  90  }},
    {"180", { "180°", 180 }},
    {"270", { "270°", 270 }},
};

int get_rotation_index(const std::string &name)
{
    for (uint32_t i = 0; i < rotation_defs.size(); i++)
    {
        if (rotation_defs[i].first == name)
        {
            return i;
        }
    }

    return 0;
}

} // namespace

std::string get_valid_rotation(const std::string &rotation)
{
    return rotation_defs[get_rotation_index(rotation)].first;
}

std::string get_prev_rotation(const std::string &rotation)
{
    int index = get_rotation_index(rotation);
    index = (index + rotation_defs.size() - 1) % rotation_defs.size();

    return rotation_defs[index].first;
}

std::string get_next_rotation(const std::string &rotation)
{
    int index = get_rotation_index(rotation);
    index = (index + 1) % rotation_defs.size();

    return rotation_defs[index].first;
}

const std::string &get_rotation_display_name(const std::string &rotation)
{
    return rotation_defs[get_rotation_index(rotation)].second.display_name;
}

int get_rotation_degrees(const std::string &rotation)
{
    return rotation_defs[get_rotation_index(rotation)].second.degrees;
}
