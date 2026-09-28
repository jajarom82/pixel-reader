#ifndef SETTINGS_VIEW_H_
#define SETTINGS_VIEW_H_

#include "reader/view.h"

#include <SDL/SDL_ttf.h>

#include <functional>
#include <string>
#include <vector>

struct SystemStyling;
struct TokenViewStyling;

class SettingsView: public View
{
    // A single adjustable menu row. Rows are data so new settings can be
    // added without growing a hand-written if/else chain per feature.
    struct Row
    {
        std::function<std::string()> get_label;
        std::function<std::string()> get_value;
        std::function<void(int dir)> on_change;
        // Optional: font to render the value in (e.g. font-name preview).
        // Null means use the system font.
        std::function<TTF_Font *()> get_value_font;
    };

    bool _is_done = false;
    bool needs_render = true;
    uint32_t line_selected = 0;
    uint32_t scroll_position = 0;
    std::string font_name;

    SystemStyling &sys_styling;
    TokenViewStyling &token_view_styling;
    uint32_t styling_sub_id;

    std::vector<Row> rows;

    void on_change_theme(int dir);
    void on_change_font_size(int dir);
    void on_change_font_name(int dir);
    void on_change_shoulder_keymap(int dir);
    void on_change_rotation(int dir);
    void on_change_custom_color(bool is_background, int channel, int dir);
    void on_change_progress(int dir);

public:
    SettingsView(
        SystemStyling &sys_styling,
        TokenViewStyling &token_view_styling,
        std::string font_name
    );
    virtual ~SettingsView();

    bool render(SDL_Surface *dest, bool force_render) override;
    bool is_done() override;
    bool is_modal() override;
    void on_keypress(SDLKey key) override;

    void terminate();
    void unterminate();
};

#endif
