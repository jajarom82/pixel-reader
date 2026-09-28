#include <iostream>
#include "./selection_menu.h"

#include "sys/screen.h"
#include "sys/keymap.h"
#include "reader/shoulder_keymap.h"
#include "reader/system_styling.h"
#include "util/sdl_utils.h"

#include <algorithm>

namespace
{

// Marquee timing for a highlighted row whose title doesn't fit: pause at
// the start, scroll left to reveal the rest, pause at the end, then loop.
constexpr uint32_t MARQUEE_START_PAUSE_MS = 1000;
constexpr uint32_t MARQUEE_END_PAUSE_MS = 600;
constexpr float MARQUEE_PX_PER_SEC = 40.0f;

// Pure function of elapsed time - no persistent state needed beyond a
// Timer that gets reset whenever the highlighted row changes.
int compute_marquee_offset(uint32_t elapsed_ms, int overflow_px)
{
    if (overflow_px <= 0)
    {
        return 0;
    }

    uint32_t scroll_ms = static_cast<uint32_t>(overflow_px / MARQUEE_PX_PER_SEC * 1000.0f);
    uint32_t cycle_ms = MARQUEE_START_PAUSE_MS + scroll_ms + MARQUEE_END_PAUSE_MS;
    uint32_t t = elapsed_ms % cycle_ms;

    if (t < MARQUEE_START_PAUSE_MS)
    {
        return 0;
    }
    t -= MARQUEE_START_PAUSE_MS;

    if (t < scroll_ms)
    {
        return static_cast<int>(t / 1000.0f * MARQUEE_PX_PER_SEC);
    }

    return overflow_px;
}

} // namespace

uint32_t SelectionMenu::num_display_lines() const
{
    return SCREEN_HEIGHT / line_height;
}

uint32_t SelectionMenu::excess_pxl_y() const
{
    return SCREEN_HEIGHT - num_display_lines() * line_height;
}

SelectionMenu::SelectionMenu(SystemStyling &styling)
    : SelectionMenu({}, styling)
{
}

SelectionMenu::SelectionMenu(std::vector<MenuEntry> entries, SystemStyling &styling)
    : entries(entries),
      styling(styling),
      styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
          needs_render = true;
          int new_line_height = detect_line_height(
              this->styling.get_font_name(),
              this->styling.get_font_size()
          ) + line_padding;
          if (new_line_height != line_height)
          {
              line_height = new_line_height;
              set_cursor_pos(cursor_pos);
          }
      })),
      line_height(detect_line_height(
          styling.get_font_name(),
          styling.get_font_size()
      ) + line_padding),
      scroll_throttle(250, 100)
{
}

SelectionMenu::~SelectionMenu()
{
    styling.unsubscribe_from_changes(styling_sub_id);
}

void SelectionMenu::set_entries(std::vector<MenuEntry> new_entries)
{
    entries = new_entries;
    set_cursor_pos(0);
    needs_render = true;
}

void SelectionMenu::set_on_selection(std::function<void(uint32_t)> callback)
{
    on_selection = callback;
}

void SelectionMenu::set_on_focus(std::function<void(uint32_t)> callback)
{
    on_focus = callback;
}

void SelectionMenu::set_default_on_keypress(std::function<void(SDLKey, SelectionMenu &)> callback)
{
    default_on_keypress = callback;
}

void SelectionMenu::set_close_on_select()
{
    close_on_select = true;
}

void SelectionMenu::set_cursor_pos(const std::string &entry)
{
    for (uint32_t i = 0; i < entries.size(); ++i)
    {
        if (entries[i].text == entry)
        {
            set_cursor_pos(i);
            break;
        }
    }
}

void SelectionMenu::set_cursor_pos(uint32_t new_cursor_pos)
{
    if (new_cursor_pos >= entries.size())
    {
        new_cursor_pos = 0;
    }

    cursor_pos = new_cursor_pos;
    marquee_timer.reset();
    if (on_focus)
    {
        on_focus(cursor_pos);
    }

    int num_lines = num_display_lines();
    int num_entries = entries.size();
    scroll_pos = std::max(
        0,
        std::min(
            num_entries - num_lines,
            static_cast<int>(new_cursor_pos) - num_lines / 4 - 1
        )
    );

    needs_render = true;
}

void SelectionMenu::close()
{
    _is_done = true;
}

bool SelectionMenu::render(SDL_Surface *dest_surface, bool force_render)
{
    if (!needs_render && !force_render)
    {
        return false;
    }
    needs_render = false;

    TTF_Font *loaded_font = styling.get_loaded_font();

    const SDL_PixelFormat *pixel_format = dest_surface->format;

    const auto &theme = styling.get_loaded_color_theme();
    const SDL_Color &fg_color = theme.main_text;
    const SDL_Color &bg_color = theme.background;
    const SDL_Color &hl_bg_color = theme.highlight_background;
    const SDL_Color &hl_text_color = theme.highlight_text;

    uint32_t rect_bg_color = SDL_MapRGB(pixel_format, bg_color.r, bg_color.g, bg_color.b);
    uint32_t rect_highlight_color = SDL_MapRGB(pixel_format, hl_bg_color.r, hl_bg_color.g, hl_bg_color.b);

    Sint16 x = line_padding;
    Sint16 y = excess_pxl_y() / 2;

    // Clear screen
    SDL_Rect rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_FillRect(dest_surface, &rect, rect_bg_color);

    // Draw lines
    bool row_needs_marquee = false;
    uint32_t num_lines = num_display_lines();
    for (uint32_t i = 0; i < num_lines; ++i)
    {
        uint32_t global_i = i + scroll_pos;
        if (global_i >= entries.size())
        {
            break;
        }

        const auto &entry = entries[global_i];

        bool is_highlighted = (global_i == cursor_pos);

        // Draw hightlight
        if (is_highlighted)
        {
            SDL_Rect rect = {0, y, SCREEN_WIDTH, (Uint16)(line_height)};
            SDL_FillRect(dest_surface, &rect, rect_highlight_color);
        }

        // Pre-render the right-aligned label (if any) first, so the title
        // below knows how much width to leave for it and never draws over it.
        surface_unique_ptr label_surface;
        int reserved_w = 0;
        if (!entry.right_label.empty())
        {
            label_surface = surface_unique_ptr { TTF_RenderUTF8_Shaded(
                loaded_font,
                entry.right_label.c_str(),
                is_highlighted ? hl_text_color : theme.secondary_text,
                is_highlighted ? hl_bg_color : bg_color
            ) };
            if (label_surface)
            {
                reserved_w = label_surface->w + line_padding;
            }
        }
        int avail_w = std::max(0, SCREEN_WIDTH - x - line_padding - reserved_w);

        // Draw text - directories are styled distinctly (secondary color +
        // trailing slash, ls -F style); books with progress get an accent
        // color so they're obviously "in progress" even when the title
        // itself is too long to show the "% at the end" at a glance.
        {
            std::string display_text = entry.is_directory ? entry.text + "/" : entry.text;

            SDL_Color text_color = is_highlighted ?
                hl_text_color :
                (entry.is_directory ?
                    theme.secondary_text :
                    (!entry.right_label.empty() ? theme.highlight_background : fg_color));

            auto message = surface_unique_ptr { TTF_RenderUTF8_Shaded(
                loaded_font,
                display_text.c_str(),
                text_color,
                is_highlighted ? hl_bg_color : bg_color
            ) };

            if (message)
            {
                SDL_Rect dest_rect = {
                    x,
                    static_cast<Sint16>(y + line_padding / 2),
                    0, 0
                };

                if (message->w <= avail_w)
                {
                    SDL_BlitSurface(message.get(), NULL, dest_surface, &dest_rect);
                }
                else if (is_highlighted)
                {
                    // Doesn't fit and this is the selected row - scroll it
                    // so the full title becomes readable over time.
                    row_needs_marquee = true;
                    int overflow = message->w - avail_w;
                    int offset = compute_marquee_offset(marquee_timer.elapsed_ms(), overflow);
                    SDL_Rect src_rect = {
                        static_cast<Sint16>(offset),
                        0,
                        static_cast<Uint16>(avail_w),
                        static_cast<Uint16>(message->h)
                    };
                    SDL_BlitSurface(message.get(), &src_rect, dest_surface, &dest_rect);
                }
                else
                {
                    // Not selected - just clip to leave room for the label,
                    // rather than potentially drawing over it.
                    SDL_Rect src_rect = {0, 0, static_cast<Uint16>(avail_w), static_cast<Uint16>(message->h)};
                    SDL_BlitSurface(message.get(), &src_rect, dest_surface, &dest_rect);
                }
            }
        }

        // Draw right-aligned label (e.g. read %) on top, if any.
        if (label_surface)
        {
            SDL_Rect label_rect = {
                static_cast<Sint16>(SCREEN_WIDTH - label_surface->w - line_padding),
                static_cast<Sint16>(y + line_padding / 2),
                0, 0
            };
            SDL_BlitSurface(label_surface.get(), NULL, dest_surface, &label_rect);
        }

        y += line_height;
    }

    current_row_needs_marquee = row_needs_marquee;

    return true;
}

void SelectionMenu::on_tick(uint32_t)
{
    if (current_row_needs_marquee)
    {
        needs_render = true;
    }
}

bool SelectionMenu::wants_continuous_render() const
{
    return current_row_needs_marquee;
}

bool SelectionMenu::is_done()
{
    return _is_done;
}

void SelectionMenu::on_move_down(uint32_t step)
{
    if (cursor_pos < entries.size() - 1)
    {
        cursor_pos = std::min(
            cursor_pos + step,
            static_cast<uint32_t>(entries.size()) - 1
        );
        marquee_timer.reset();

        if (cursor_pos >= scroll_pos + num_display_lines())
        {
            scroll_pos = cursor_pos - num_display_lines() + 1;
        }
        if (on_focus)
        {
            on_focus(cursor_pos);
        }

        needs_render = true;
    }
}

void SelectionMenu::on_move_up(uint32_t step)
{
    if (cursor_pos > 0)
    {
        cursor_pos = cursor_pos <= step ? 0 : cursor_pos - step;
        scroll_pos = std::min(scroll_pos, cursor_pos);
        marquee_timer.reset();

        if (on_focus)
        {
            on_focus(cursor_pos);
        }

        needs_render = true;
    }
}

void SelectionMenu::on_select_entry()
{
    if (!entries.empty() && on_selection)
    {
        on_selection(cursor_pos);
    }

    if (close_on_select)
    {
        _is_done = true;
    }
}

void SelectionMenu::on_keypress(SDLKey key)
{
    switch (key) {
        case SW_BTN_UP:
            on_move_up(1);
            break;
        case SW_BTN_DOWN:
            on_move_down(1);
            break;
        case SW_BTN_L1:
        case SW_BTN_R1:
        case SW_BTN_L2:
        case SW_BTN_R2:
            {
                auto [l_key, r_key] = get_shoulder_keymap_lr(
                    styling.get_shoulder_keymap()
                );

                if (key == l_key)
                {
                    key = SW_BTN_LEFT;
                }
                else if (key == r_key)
                {
                    key = SW_BTN_RIGHT;
                }
            }
            // fallthrough
        case SW_BTN_LEFT:
        case SW_BTN_RIGHT:
            if (key == SW_BTN_LEFT)
            {
                on_move_up(num_display_lines() / 2);
            }
            else if (key == SW_BTN_RIGHT)
            {
                on_move_down(num_display_lines() / 2);
            }
            break;
        case SW_BTN_A:
            on_select_entry();
            break;
        case SW_BTN_B:
            _is_done = true;
            break;
        default:
            if (default_on_keypress)
            {
                default_on_keypress(key, *this);
            }
            break;
    }
}

void SelectionMenu::on_keyheld(SDLKey key, uint32_t held_time_ms)
{
    switch (key) {
        case SW_BTN_UP:
        case SW_BTN_DOWN:
        case SW_BTN_LEFT:
        case SW_BTN_RIGHT:
        case SW_BTN_L1:
        case SW_BTN_R1:
        case SW_BTN_L2:
        case SW_BTN_R2:
            if (scroll_throttle(held_time_ms))
            {
                on_keypress(key);
            }
            break;
        default:
            break;
    }
}
