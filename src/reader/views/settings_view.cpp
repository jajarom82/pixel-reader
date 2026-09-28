#include "./settings_view.h"
#include "./token_view/token_view_styling.h"

#include "reader/color_theme_def.h"
#include "reader/config.h"
#include "reader/draw_modal_border.h"
#include "reader/font_catalog.h"
#include "reader/rotation.h"
#include "reader/settings_store.h"
#include "reader/shoulder_keymap.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_utils.h"

#include <algorithm>
#include <filesystem>

SettingsView::SettingsView(
    SystemStyling &sys_styling,
    TokenViewStyling &token_view_styling,
    std::string font_name
) : font_name(font_name),
    sys_styling(sys_styling),
    token_view_styling(token_view_styling),
    styling_sub_id(sys_styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
        needs_render = true;
    }))
{
    rows.push_back({
        [] { return std::string("Theme:"); },
        [this] { return sys_styling.get_color_theme(); },
        [this](int dir) { on_change_theme(dir); },
        nullptr
    });
    rows.push_back({
        [] { return std::string("Font size:"); },
        [this] { return std::to_string(sys_styling.get_font_size()); },
        [this](int dir) { on_change_font_size(dir); },
        nullptr
    });
    rows.push_back({
        [] { return std::string("Font:"); },
        [this] { return std::filesystem::path(sys_styling.get_font_name()).filename().stem().string(); },
        [this](int dir) { on_change_font_name(dir); },
        [this]() -> TTF_Font * { return sys_styling.get_loaded_font(); }
    });
    rows.push_back({
        [] { return std::string("Shoulder keymap:"); },
        [this] { return get_shoulder_keymap_display_name(sys_styling.get_shoulder_keymap()); },
        [this](int dir) { on_change_shoulder_keymap(dir); },
        nullptr
    });
    rows.push_back({
        [] { return std::string("Rotation:"); },
        [this] { return get_rotation_display_name(sys_styling.get_rotation()); },
        [this](int dir) { on_change_rotation(dir); },
        nullptr
    });
    rows.push_back({
        [] { return std::string("Progress:"); },
        [this] {
            return std::string(
                token_view_styling.get_progress_reporting() == ProgressReporting::CHAPTER_PERCENT ?
                    "Chapter %" :
                    "Book %"
            );
        },
        [this](int dir) { on_change_progress(dir); },
        nullptr
    });
}

SettingsView::~SettingsView()
{
    sys_styling.unsubscribe_from_changes(styling_sub_id);
}

bool SettingsView::render(SDL_Surface *dest_surface, bool force_render)
{
    if (needs_render || force_render)
    {
        TTF_Font *sys_font = cached_load_font(font_name, sys_styling.get_font_size());
        const auto &theme = sys_styling.get_loaded_color_theme();

        constexpr int style_normal = 0;
        constexpr int style_hl = 1;
        constexpr int style_label = 2;

        auto render_text = [&](const std::string &str, int style, TTF_Font *font = nullptr) {
            return surface_unique_ptr { TTF_RenderUTF8_Shaded(
                font ? font : sys_font,
                str.c_str(),
                style == style_normal ?
                    theme.main_text :
                    (style == style_hl ? theme.highlight_text : theme.secondary_text),
                style == style_normal ?
                    theme.background :
                    (style == style_hl ? theme.highlight_background : theme.background)
            ) };
        };

        auto left_arrow = render_text("◂", style_hl);
        auto right_arrow = render_text("▸", style_hl);

        struct RenderedRow
        {
            surface_unique_ptr label;
            surface_unique_ptr value;
        };

        std::vector<RenderedRow> rendered_rows;
        for (uint32_t i = 0; i < rows.size(); ++i)
        {
            rendered_rows.push_back({
                render_text(rows[i].get_label(), style_label),
                render_text(
                    rows[i].get_value(),
                    line_selected == i ? style_hl : style_normal,
                    rows[i].get_value_font ? rows[i].get_value_font() : nullptr
                )
            });
        }

        int num_menu_items = static_cast<int>(rows.size());

        Uint16 content_w;
        {
            int arrow_w = left_arrow->w + right_arrow->w;
            std::vector<int> widths;
            for (const auto &row : rendered_rows)
            {
                widths.push_back(row.label->w);
                widths.push_back(row.value->w + arrow_w);
            }
            content_w = *std::max_element(widths.begin(), widths.end());
        }

        Uint16 text_padding = 5;
        Uint16 max_content_h = SCREEN_HEIGHT - DIALOG_BORDER_WIDTH * 2;
        Uint16 line_height = rendered_rows[0].label->h + rendered_rows[0].value->h;
        int max_lines = std::max(1, (max_content_h + text_padding) / (line_height + text_padding));
        int num_lines_shown = std::min(num_menu_items, max_lines);
        {
            if (max_lines >= num_menu_items)
            {
                scroll_position = 0;
            }
            else if (line_selected < scroll_position)
            {
                // scroll up
                scroll_position = line_selected;
            }
            else if (line_selected >= scroll_position + max_lines - 1)
            {
                // scroll down
                scroll_position = line_selected + 1 - max_lines;
            }
        }

        Uint16 content_h = num_lines_shown * line_height + (num_lines_shown - 1) * text_padding;
        Sint16 content_y = SCREEN_HEIGHT / 2 - content_h / 2;

        draw_modal_border(
            content_w,
            content_h,
            theme,
            dest_surface
        );

        // draw text
        {
            SDL_Rect rect = {0, content_y, 0, 0};
            auto push_text = [&](SDL_Surface *surf, bool add_arrows = false) {
                Sint16 start = SCREEN_WIDTH / 2 - surf->w / 2;

                rect.x = start;
                SDL_BlitSurface(surf, NULL, dest_surface, &rect);

                if (add_arrows)
                {
                    SDL_Rect arrow_rect = {0, 0, 0, 0};
                    arrow_rect.y = rect.y + (surf->h - left_arrow->h) / 2;

                    arrow_rect.x = start - left_arrow->w;
                    SDL_BlitSurface(left_arrow.get(), NULL, dest_surface, &arrow_rect);

                    arrow_rect.x = start + surf->w;
                    SDL_BlitSurface(right_arrow.get(), NULL, dest_surface, &arrow_rect);
                }

                rect.y += surf->h;
            };

            auto is_line_shown = [this, num_lines_shown](uint32_t i) {
                return i >= scroll_position && i <= scroll_position + num_lines_shown - 1;
            };

            uint32_t rendered_count = 0;
            for (uint32_t i = 0; i < rows.size(); ++i)
            {
                if (is_line_shown(i))
                {
                    push_text(rendered_rows[i].label.get());
                    push_text(rendered_rows[i].value.get(), line_selected == i);

                    ++rendered_count;
                    if (rendered_count < static_cast<uint32_t>(num_lines_shown))
                    {
                        rect.y += text_padding;
                    }
                }
            }
        }

        needs_render = false;
        return true;
    }

    return false;
}

bool SettingsView::is_done()
{
    return _is_done;
}

bool SettingsView::is_modal()
{
    return true;
}

void SettingsView::on_change_theme(int dir)
{
    std::string theme = sys_styling.get_color_theme();
    sys_styling.set_color_theme(
        (dir < 0) ?
            get_prev_theme(theme) :
            get_next_theme(theme)
    );
}

void SettingsView::on_change_font_size(int dir)
{
    sys_styling.set_font_size(
        (dir < 0) ?
            sys_styling.get_prev_font_size() :
            sys_styling.get_next_font_size()
    );
}

void SettingsView::on_change_font_name(int dir)
{
    std::string font_name = sys_styling.get_font_name();
    sys_styling.set_font_name(
        (dir < 0) ?
            get_prev_font_name(font_name) :
            get_next_font_name(font_name)
    );
}

void SettingsView::on_change_shoulder_keymap(int dir)
{
    const auto &keymap = sys_styling.get_shoulder_keymap();
    sys_styling.set_shoulder_keymap(
        (dir < 0) ?
            get_prev_shoulder_keymap(keymap) :
            get_next_shoulder_keymap(keymap)
    );
}

void SettingsView::on_change_rotation(int dir)
{
    const auto &rotation = sys_styling.get_rotation();
    sys_styling.set_rotation(
        (dir < 0) ?
            get_prev_rotation(rotation) :
            get_next_rotation(rotation)
    );
}

void SettingsView::on_change_progress(int)
{
    token_view_styling.set_progress_reporting(
        get_next_progress_reporting(token_view_styling.get_progress_reporting())
    );
}

void SettingsView::on_keypress(SDLKey key)
{
    uint32_t num_rows = static_cast<uint32_t>(rows.size());

    switch (key) {
        case SW_BTN_UP:
            line_selected = (line_selected + num_rows - 1) % num_rows;
            needs_render = true;
            break;
        case SW_BTN_DOWN:
            line_selected = (line_selected + 1) % num_rows;
            needs_render = true;
            break;
        case SW_BTN_LEFT:
        case SW_BTN_RIGHT:
            {
                int dir = (key == SW_BTN_LEFT) ? -1 : 1;
                rows[line_selected].on_change(dir);
                needs_render = true;
            }
            break;
        case SW_BTN_B:
            {
                _is_done = true;
            }
            break;
        default:
            break;
    }
}

void SettingsView::terminate()
{
    _is_done = true;
}

void SettingsView::unterminate()
{
    _is_done = false;
}
