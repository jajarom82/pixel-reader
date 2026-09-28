#include "./settings_store.h"
#include "./state_store.h"
#include "./util/string_serialization.h"

#include <iostream>
#include <vector>

namespace
{

constexpr const char *SETTINGS_KEY_SHOW_TITLE_BAR = "show_title_bar";
constexpr const char *SETTINGS_SHOULDER_KEYMAP = "shoulder_keymap";
constexpr const char *SETTINGS_KEY_COLOR_THEME = "color_theme";
constexpr const char *SETTINGS_KEY_FONT_NAME = "font_name";
constexpr const char *SETTINGS_KEY_FONT_SIZE = "font_size";
constexpr const char *SETTINGS_KEY_AUTO_SCROLL_SPEED = "auto_scroll_speed";
constexpr const char *SETTINGS_PROGRESS_REPORTING = "progress_reporting";
constexpr const char *SETTINGS_KEY_ROTATION = "rotation";

constexpr const char *SETTINGS_KEY_CUSTOM_BG_R = "custom_bg_r";
constexpr const char *SETTINGS_KEY_CUSTOM_BG_G = "custom_bg_g";
constexpr const char *SETTINGS_KEY_CUSTOM_BG_B = "custom_bg_b";
constexpr const char *SETTINGS_KEY_CUSTOM_FG_R = "custom_fg_r";
constexpr const char *SETTINGS_KEY_CUSTOM_FG_G = "custom_fg_g";
constexpr const char *SETTINGS_KEY_CUSTOM_FG_B = "custom_fg_b";
constexpr const char *SETTINGS_KEY_BROWSE_VIEW_MODE = "browse_view_mode";

std::optional<SDL_Color> get_color_setting(
    const StateStore &state_store,
    const char *r_key,
    const char *g_key,
    const char *b_key
)
{
    auto r = state_store.get_setting(r_key);
    auto g = state_store.get_setting(g_key);
    auto b = state_store.get_setting(b_key);
    if (!r || !g || !b)
    {
        return std::nullopt;
    }

    auto r_val = try_decode_uint(*r);
    auto g_val = try_decode_uint(*g);
    auto b_val = try_decode_uint(*b);
    if (!r_val || !g_val || !b_val)
    {
        return std::nullopt;
    }

    SDL_Color color = {
        static_cast<Uint8>(*r_val),
        static_cast<Uint8>(*g_val),
        static_cast<Uint8>(*b_val),
        0
    };
    return color;
}

void set_color_setting(
    StateStore &state_store,
    const char *r_key,
    const char *g_key,
    const char *b_key,
    SDL_Color color
)
{
    state_store.set_setting(r_key, std::to_string(color.r));
    state_store.set_setting(g_key, std::to_string(color.g));
    state_store.set_setting(b_key, std::to_string(color.b));
}

} // namespace

std::optional<bool> settings_get_show_title_bar(const StateStore &state_store)
{
    const auto &opt = state_store.get_setting(SETTINGS_KEY_SHOW_TITLE_BAR);
    if (!opt)
    {
        return std::nullopt;
    }
    return *opt == "true";
}

void settings_set_show_title_bar(StateStore &state_store, bool show_title_bar)
{
    state_store.set_setting(SETTINGS_KEY_SHOW_TITLE_BAR, show_title_bar ? "true" : "false");
}

std::optional<std::string> settings_get_shoulder_keymap(const StateStore &state_store)
{
    return state_store.get_setting(SETTINGS_SHOULDER_KEYMAP);
}

void settings_set_shoulder_keymap(StateStore &state_store, std::string keymap)
{
    state_store.set_setting(SETTINGS_SHOULDER_KEYMAP, keymap);
}

std::optional<ProgressReporting> settings_get_progress_reporting(const StateStore &state_store)
{
    auto progress = state_store.get_setting(SETTINGS_PROGRESS_REPORTING);
    if (!progress)
    {
        return std::nullopt;
    }

    return decode_progress_reporting(*progress);
}

void settings_set_progress_reporting(StateStore &state_store, ProgressReporting progress_reporting)
{
    state_store.set_setting(SETTINGS_PROGRESS_REPORTING, encode_progress_reporting(progress_reporting));
}

std::optional<std::string> settings_get_color_theme(const StateStore &state_store)
{
    return state_store.get_setting(SETTINGS_KEY_COLOR_THEME);
}

void settings_set_color_theme(StateStore &state_store, const std::string &color_theme)
{
    state_store.set_setting(SETTINGS_KEY_COLOR_THEME, color_theme);
}

std::optional<std::string> settings_get_font_name(const StateStore &state_store)
{
    return state_store.get_setting(SETTINGS_KEY_FONT_NAME);
}

void settings_set_font_name(StateStore &state_store, const std::string &font_name)
{
    state_store.set_setting(SETTINGS_KEY_FONT_NAME, font_name);
}

std::optional<uint32_t> settings_get_font_size(const StateStore &state_store)
{
    auto font_size = state_store.get_setting(SETTINGS_KEY_FONT_SIZE);
    if (!font_size)
    {
        return std::nullopt;
    }
    return try_decode_uint(*font_size);
}

void settings_set_font_size(StateStore &state_store, uint32_t font_size)
{
    state_store.set_setting(SETTINGS_KEY_FONT_SIZE, std::to_string(font_size));
}

std::optional<uint32_t> settings_get_auto_scroll_speed(const StateStore &state_store)
{
    auto speed = state_store.get_setting(SETTINGS_KEY_AUTO_SCROLL_SPEED);
    if (!speed)
    {
        return std::nullopt;
    }
    return try_decode_uint(*speed);
}

void settings_set_auto_scroll_speed(StateStore &state_store, uint32_t speed)
{
    state_store.set_setting(SETTINGS_KEY_AUTO_SCROLL_SPEED, std::to_string(speed));
}

std::optional<std::string> settings_get_rotation(const StateStore &state_store)
{
    return state_store.get_setting(SETTINGS_KEY_ROTATION);
}

void settings_set_rotation(StateStore &state_store, const std::string &rotation)
{
    state_store.set_setting(SETTINGS_KEY_ROTATION, rotation);
}

std::optional<SDL_Color> settings_get_custom_background_color(const StateStore &state_store)
{
    return get_color_setting(state_store, SETTINGS_KEY_CUSTOM_BG_R, SETTINGS_KEY_CUSTOM_BG_G, SETTINGS_KEY_CUSTOM_BG_B);
}

void settings_set_custom_background_color(StateStore &state_store, SDL_Color color)
{
    set_color_setting(state_store, SETTINGS_KEY_CUSTOM_BG_R, SETTINGS_KEY_CUSTOM_BG_G, SETTINGS_KEY_CUSTOM_BG_B, color);
}

std::optional<SDL_Color> settings_get_custom_main_text_color(const StateStore &state_store)
{
    return get_color_setting(state_store, SETTINGS_KEY_CUSTOM_FG_R, SETTINGS_KEY_CUSTOM_FG_G, SETTINGS_KEY_CUSTOM_FG_B);
}

void settings_set_custom_main_text_color(StateStore &state_store, SDL_Color color)
{
    set_color_setting(state_store, SETTINGS_KEY_CUSTOM_FG_R, SETTINGS_KEY_CUSTOM_FG_G, SETTINGS_KEY_CUSTOM_FG_B, color);
}

std::optional<std::string> settings_get_browse_view_mode(const StateStore &state_store)
{
    return state_store.get_setting(SETTINGS_KEY_BROWSE_VIEW_MODE);
}

void settings_set_browse_view_mode(StateStore &state_store, const std::string &mode)
{
    state_store.set_setting(SETTINGS_KEY_BROWSE_VIEW_MODE, mode);
}
