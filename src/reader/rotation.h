#ifndef ROTATION_H_
#define ROTATION_H_

#include <string>

std::string get_valid_rotation(const std::string &rotation);
std::string get_prev_rotation(const std::string &rotation);
std::string get_next_rotation(const std::string &rotation);

const std::string &get_rotation_display_name(const std::string &rotation);

// Degrees clockwise the rendered content is rotated before being presented
// on the physical panel (0, 90, 180 or 270).
int get_rotation_degrees(const std::string &rotation);

#endif
