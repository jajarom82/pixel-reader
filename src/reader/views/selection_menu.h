#ifndef SELECTION_MENU_H_
#define SELECTION_MENU_H_

#include "reader/view.h"
#include "util/throttled.h"
#include "util/timer.h"

#include <SDL/SDL_ttf.h>

#include <functional>
#include <string>
#include <vector>

struct SystemStyling;

// A single menu row. Directories get styled differently (Type::Directory)
// so they stand out from files in the list; progress_percent (0 = not
// started / not applicable) is drawn as a thin bar below the title.
struct MenuEntry
{
    std::string text;
    bool is_directory = false;
    uint32_t progress_percent = 0;

    // Implicit: lets existing callers keep passing plain std::string/vector<std::string>.
    MenuEntry(std::string text) : text(std::move(text)) {}

    MenuEntry(std::string text, bool is_directory, uint32_t progress_percent = 0)
        : text(std::move(text)), is_directory(is_directory), progress_percent(progress_percent) {}
};

class SelectionMenu: public View
{
    bool needs_render = true;

    std::vector<MenuEntry> entries;
    uint32_t cursor_pos = 0;
    uint32_t scroll_pos = 0;
    bool close_on_select = false;

    SystemStyling &styling;
    const uint32_t styling_sub_id;

    const int line_padding = 4;
    int line_height;
    uint32_t num_display_lines() const;
    uint32_t excess_pxl_y() const;

    Throttled scroll_throttle;

    // Marquee: when the highlighted row's title is too wide to fit, scroll
    // it back and forth over time instead of silently clipping it, so the
    // full title stays readable.
    Timer marquee_timer;
    bool current_row_needs_marquee = false;

    bool _is_done = false;
    std::function<void(uint32_t)> on_selection;
    std::function<void(uint32_t)> on_focus;
    std::function<void(SDLKey, SelectionMenu&)> default_on_keypress;

    void on_move_down(uint32_t step);
    void on_move_up(uint32_t step);
    void on_select_entry();

public:

    SelectionMenu(SystemStyling &styling);
    SelectionMenu(std::vector<MenuEntry> entries, SystemStyling &styling);
    virtual ~SelectionMenu();

    void set_entries(std::vector<MenuEntry> new_entries);
    void set_on_selection(std::function<void(uint32_t)> callback);
    void set_on_focus(std::function<void(uint32_t)> callback);
    // Define fallback keypress handler
    void set_default_on_keypress(std::function<void(SDLKey, SelectionMenu &)> callback);
    void set_close_on_select();

    void set_cursor_pos(const std::string &entry);
    void set_cursor_pos(uint32_t pos);
    uint32_t get_cursor_pos() const;

    void close();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    void on_keypress(SDLKey key) override;
    void on_keyheld(SDLKey key, uint32_t held_time_ms) override;
    void on_tick(uint32_t elapsed_ms) override;
    bool wants_continuous_render() const override;
};

#endif
