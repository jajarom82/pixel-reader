#include "./color_theme_def.h"
#include "./config.h"

#include <string>
#include <utility>
#include <vector>

namespace
{

const std::vector<std::pair<std::string, ColorTheme>> theme_defs = {
    {
        "night_contrast",
        {
            {0, 0, 0, 0},       // background
            {240, 240, 240, 0}, // main text
            {96, 96, 96, 0},    // secondary text
            {150, 38, 200, 0},  // highlight background
            {240, 240, 240, 0}, // highlight text
        }
    },
    {
        "light_contrast",
        {
            {255, 255, 255, 0}, // background
            {0, 0, 0, 0},       // main text
            {160, 160, 160, 0}, // secondary text
            {163, 81, 200, 0},  // highlight background
            {250, 250, 250, 0}, // highlight text
        }
    },
    {
        "light_sepia",
        {
            {250, 240, 220, 0}, // background
            {0, 0, 0, 0},    // main text
            {160, 160, 160, 0}, // secondary text
            {163, 81, 200, 0},  // highlight background
            {250, 240, 220, 0}, // highlight text
        }
    },
    {
        "vampire",
        {
            {0, 0, 0, 0},   // background
            {192, 0, 0, 0}, // main text
            {96, 0, 0, 0},  // secondary text
            {192, 0, 0, 0}, // highlight background
            {0, 0, 0, 0},   // highlight text
        }
    },
};

const char *CUSTOM_THEME_NAME = "custom";

// Mutable: populated via set_custom_theme_colors(), initially unused until
// the user picks colors (or a persisted choice is loaded at startup).
ColorTheme custom_theme = {
    {0, 0, 0, 0},       // background
    {240, 240, 240, 0}, // main text
    {120, 120, 120, 0}, // secondary text
    {240, 240, 240, 0}, // highlight background
    {0, 0, 0, 0},       // highlight text
};

int get_theme_index(const std::string &name)
{
    for (uint32_t i = 0; i < theme_defs.size(); i++)
    {
        if (theme_defs[i].first == name)
        {
            return i;
        }
    }

    return -1;
}

} // namespace

const ColorTheme& get_color_theme(const std::string &name)
{
    if (name == CUSTOM_THEME_NAME)
    {
        return custom_theme;
    }

    int i = get_theme_index(name);
    if (i < 0)
    {
        i = 0;
    }

    return theme_defs[i].second;
}

std::string get_valid_theme(const std::string &preferred)
{
    if (preferred == CUSTOM_THEME_NAME)
    {
        return preferred;
    }

    int i = get_theme_index(preferred);
    if (i < 0)
    {
        i = get_theme_index(DEFAULT_COLOR_THEME);
    }
    if (i < 0)
    {
        i = 0;
    }
    return theme_defs[i].first;
}

std::string get_prev_theme(const std::string &name)
{
    if (name == CUSTOM_THEME_NAME)
    {
        return theme_defs.back().first;
    }

    int i = get_theme_index(name);
    if (i < 0)
    {
        return theme_defs[0].first;
    }

    if (i == 0)
    {
        return CUSTOM_THEME_NAME;
    }

    return theme_defs[i - 1].first;
}

std::string get_next_theme(const std::string &name)
{
    if (name == CUSTOM_THEME_NAME)
    {
        return theme_defs[0].first;
    }

    int i = get_theme_index(name);
    if (i < 0)
    {
        return theme_defs[0].first;
    }

    if (static_cast<uint32_t>(i) == theme_defs.size() - 1)
    {
        return CUSTOM_THEME_NAME;
    }

    return theme_defs[i + 1].first;
}

void set_custom_theme_colors(SDL_Color background, SDL_Color main_text)
{
    SDL_Color secondary_text = {
        static_cast<Uint8>((background.r + main_text.r) / 2),
        static_cast<Uint8>((background.g + main_text.g) / 2),
        static_cast<Uint8>((background.b + main_text.b) / 2),
        0
    };

    // Highlight swaps fg/bg so the selection bar reads clearly regardless
    // of what colors were picked.
    custom_theme = ColorTheme{
        background,
        main_text,
        secondary_text,
        main_text,
        background,
    };
}

SDL_Color get_custom_background_color()
{
    return custom_theme.background;
}

SDL_Color get_custom_main_text_color()
{
    return custom_theme.main_text;
}
