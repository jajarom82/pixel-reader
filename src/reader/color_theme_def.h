#ifndef COLOR_THEME_DEF_H_
#define COLOR_THEME_DEF_H_

#include "./color_theme.h"
#include <string>

const ColorTheme& get_color_theme(const std::string &name);
std::string get_valid_theme(const std::string &preferred);
std::string get_prev_theme(const std::string &name);
std::string get_next_theme(const std::string &name);

// The "custom" theme's colors. main_text/background are user-chosen; the
// remaining ColorTheme fields (secondary text, highlight) are derived
// automatically so the UI stays legible without needing a picker per field.
void set_custom_theme_colors(SDL_Color background, SDL_Color main_text);
SDL_Color get_custom_background_color();
SDL_Color get_custom_main_text_color();

#endif
