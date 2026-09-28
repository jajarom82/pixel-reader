#include "./gallery_view.h"

#include "./browse_state.h"
#include "extern/rotozoom/SDL_rotozoom.h"
#include "filetypes/epub/epub_cover.h"
#include "reader/state_store.h"
#include "reader/system_styling.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_image_cache.h"
#include "util/sdl_utils.h"
#include "util/throttled.h"

#include <algorithm>
#include <set>
#include <vector>

namespace
{

constexpr int TILE_COLUMNS = 3;
constexpr int TILE_PADDING = 8;
constexpr float COVER_ASPECT = 1.45f; // height/width, typical book cover

} // namespace

struct GVState
{
    BrowseState browse;
    StateStore &state_store;
    SystemStyling &styling;
    const uint32_t styling_sub_id;

    std::function<void(const std::filesystem::path &)> on_file_selected;
    std::function<void(const std::filesystem::path &)> on_file_focus;
    std::function<void()> on_view_focus;

    uint32_t cursor_pos = 0;
    uint32_t scroll_row = 0;

    bool needs_render = true;
    bool _is_done = false;

    SDLImageCache cover_cache;
    std::set<std::string> no_cover_paths;

    Throttled move_throttle;

    GVState(std::filesystem::path path, SystemStyling &styling, StateStore &state_store)
        : browse(std::move(path)),
          state_store(state_store),
          styling(styling),
          styling_sub_id(styling.subscribe_to_changes([this](SystemStyling::ChangeId) {
              needs_render = true;
          })),
          move_throttle(250, 100)
    {
    }

    ~GVState()
    {
        styling.unsubscribe_from_changes(styling_sub_id);
    }
};

namespace {

uint32_t find_entry_index(const std::vector<FSEntry> &entries, const std::string &name)
{
    for (uint32_t i = 0; i < entries.size(); ++i)
    {
        if (entries[i].name == name)
        {
            return i;
        }
    }
    return 0;
}

void notify_focus(GVState *s)
{
    const auto &entries = s->browse.get_entries();
    if (!entries.empty() && s->on_file_focus && s->cursor_pos < entries.size())
    {
        s->on_file_focus(s->browse.get_path() / entries[s->cursor_pos].name);
    }
}

// Returns a cached, pre-scaled cover thumbnail for `full_path`, or nullptr
// if it has none (a folder, a non-epub file, or an epub with no declared
// cover). Failures are remembered so we don't re-open/re-parse the same
// file's zip/XML on every re-render (e.g. moving the cursor around).
SDL_Surface *load_cover_thumbnail(GVState *s, const std::filesystem::path &full_path, int max_w, int max_h)
{
    std::string key = full_path.string();

    if (SDL_Surface *cached = s->cover_cache.get_image(key))
    {
        return cached;
    }

    if (s->no_cover_paths.count(key) || full_path.extension() != ".epub")
    {
        return nullptr;
    }

    auto cover = get_epub_cover_image(full_path);
    if (!cover)
    {
        s->no_cover_paths.insert(key);
        return nullptr;
    }

    auto surface = load_surface_from_ptr(
        cover->data.data(),
        cover->data.size(),
        cover->extension,
        get_render_surface_format()
    );
    if (!surface || surface->w <= 0 || surface->h <= 0)
    {
        s->no_cover_paths.insert(key);
        return nullptr;
    }

    float scale = std::min(
        static_cast<float>(max_w) / surface->w,
        static_cast<float>(max_h) / surface->h
    );

    surface_unique_ptr final_surface = (scale > 0.001f && (scale < 0.999f || scale > 1.001f)) ?
        surface_unique_ptr { zoomSurface(surface.get(), scale, scale, 1) } :
        std::move(surface);

    if (!final_surface)
    {
        s->no_cover_paths.insert(key);
        return nullptr;
    }

    s->cover_cache.put_image(key, std::move(final_surface));
    return s->cover_cache.get_image(key);
}

void draw_placeholder_tile(SDL_Surface *dest, const SDL_Rect &rect, const ColorTheme &theme, bool is_directory)
{
    uint32_t accent = SDL_MapRGB(dest->format, theme.secondary_text.r, theme.secondary_text.g, theme.secondary_text.b);

    if (is_directory)
    {
        // Simple folder shape: a tab + a body.
        SDL_Rect tab = { rect.x, rect.y, static_cast<Uint16>(rect.w / 2), static_cast<Uint16>(rect.h / 6) };
        SDL_Rect body = { rect.x, static_cast<Sint16>(rect.y + rect.h / 6), rect.w, static_cast<Uint16>(rect.h - rect.h / 6) };
        SDL_FillRect(dest, &tab, accent);
        SDL_FillRect(dest, &body, accent);
    }
    else
    {
        // Simple document shape: an outlined rect.
        SDL_FillRect(dest, &rect, accent);
        SDL_Rect inner = {
            static_cast<Sint16>(rect.x + 3),
            static_cast<Sint16>(rect.y + 3),
            static_cast<Uint16>(std::max(0, rect.w - 6)),
            static_cast<Uint16>(std::max(0, rect.h - 6))
        };
        uint32_t bg_color = SDL_MapRGB(dest->format, theme.background.r, theme.background.g, theme.background.b);
        SDL_FillRect(dest, &inner, bg_color);
    }
}

} // namespace

GalleryView::GalleryView(std::filesystem::path path, SystemStyling &styling, StateStore &state_store)
    : state(std::make_unique<GVState>(path, styling, state_store))
{
    const auto &entries = state->browse.get_entries();
    if (path.has_filename())
    {
        state->cursor_pos = find_entry_index(entries, path.filename());
    }
    else if (entries.size() > 1)
    {
        state->cursor_pos = 1; // get past ".." entry
    }
}

GalleryView::~GalleryView()
{
}

bool GalleryView::render(SDL_Surface *dest_surface, bool force_render)
{
    if (!state->needs_render && !force_render)
    {
        return false;
    }
    state->needs_render = false;

    const auto &theme = state->styling.get_loaded_color_theme();
    TTF_Font *font = state->styling.get_loaded_font();

    uint32_t bg_color = SDL_MapRGB(dest_surface->format, theme.background.r, theme.background.g, theme.background.b);
    SDL_Rect full_rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_FillRect(dest_surface, &full_rect, bg_color);

    const auto &entries = state->browse.get_entries();
    if (entries.empty())
    {
        return true;
    }

    int tile_w = SCREEN_WIDTH / TILE_COLUMNS;
    int cover_h = static_cast<int>(tile_w * COVER_ASPECT);
    int label_h = detect_line_height(font) + 4;
    int tile_h = cover_h + label_h;
    int rows_visible = std::max(1, static_cast<int>(SCREEN_HEIGHT) / tile_h);

    uint32_t cursor_row = state->cursor_pos / TILE_COLUMNS;
    if (cursor_row < state->scroll_row)
    {
        state->scroll_row = cursor_row;
    }
    else if (cursor_row >= state->scroll_row + static_cast<uint32_t>(rows_visible))
    {
        state->scroll_row = cursor_row - rows_visible + 1;
    }

    uint32_t first_index = state->scroll_row * TILE_COLUMNS;
    uint32_t num_entries = static_cast<uint32_t>(entries.size());

    uint32_t hl_bg = SDL_MapRGB(dest_surface->format, theme.highlight_background.r, theme.highlight_background.g, theme.highlight_background.b);

    for (int row = 0; row < rows_visible; ++row)
    {
        for (int col = 0; col < TILE_COLUMNS; ++col)
        {
            uint32_t index = first_index + row * TILE_COLUMNS + col;
            if (index >= num_entries)
            {
                break;
            }

            const auto &entry = entries[index];
            bool is_highlighted = (index == state->cursor_pos);

            int x = col * tile_w;
            int y = row * tile_h;

            if (is_highlighted)
            {
                SDL_Rect hl_rect = { static_cast<Sint16>(x), static_cast<Sint16>(y), static_cast<Uint16>(tile_w), static_cast<Uint16>(tile_h) };
                SDL_FillRect(dest_surface, &hl_rect, hl_bg);
            }

            SDL_Rect cover_rect = {
                static_cast<Sint16>(x + TILE_PADDING),
                static_cast<Sint16>(y + TILE_PADDING),
                static_cast<Uint16>(std::max(0, tile_w - TILE_PADDING * 2)),
                static_cast<Uint16>(std::max(0, cover_h - TILE_PADDING * 2))
            };

            SDL_Surface *cover = entry.is_dir ?
                nullptr :
                load_cover_thumbnail(state.get(), state->browse.get_path() / entry.name, cover_rect.w, cover_rect.h);

            if (cover)
            {
                SDL_Rect dst = {
                    static_cast<Sint16>(cover_rect.x + (cover_rect.w - cover->w) / 2),
                    static_cast<Sint16>(cover_rect.y + (cover_rect.h - cover->h) / 2),
                    0, 0
                };
                SDL_BlitSurface(cover, NULL, dest_surface, &dst);
            }
            else
            {
                draw_placeholder_tile(dest_surface, cover_rect, theme, entry.is_dir);
            }

            // Label: filename (truncated to tile width), plus a progress
            // badge for files that have been started.
            {
                std::string label = entry.is_dir ? entry.name + "/" : entry.name;

                std::string right_label;
                if (!entry.is_dir)
                {
                    auto book_id = state->state_store.get_book_id_for_path(state->browse.get_path() / entry.name);
                    if (book_id)
                    {
                        auto progress = state->state_store.get_book_progress(*book_id);
                        if (progress && *progress > 0)
                        {
                            right_label = std::to_string(*progress) + "%";
                        }
                    }
                }

                int avail_w = tile_w - TILE_PADDING * 2;
                while (label.size() > 1)
                {
                    int w = 0, h = 0;
                    TTF_SizeUTF8(font, label.c_str(), &w, &h);
                    if (w <= avail_w)
                    {
                        break;
                    }
                    label.pop_back();
                }

                const SDL_Color &label_color = is_highlighted ? theme.highlight_text : (entry.is_dir ? theme.secondary_text : theme.main_text);
                const SDL_Color &label_bg = is_highlighted ? theme.highlight_background : theme.background;

                auto label_surface = surface_unique_ptr { TTF_RenderUTF8_Shaded(font, label.c_str(), label_color, label_bg) };
                SDL_Rect label_rect = {
                    static_cast<Sint16>(x + TILE_PADDING),
                    static_cast<Sint16>(y + cover_h + 2),
                    0, 0
                };
                SDL_BlitSurface(label_surface.get(), NULL, dest_surface, &label_rect);

                if (!right_label.empty())
                {
                    auto progress_surface = surface_unique_ptr { TTF_RenderUTF8_Shaded(font, right_label.c_str(), theme.secondary_text, label_bg) };
                    SDL_Rect progress_rect = {
                        static_cast<Sint16>(x + tile_w - TILE_PADDING - progress_surface->w),
                        static_cast<Sint16>(y + cover_h + 2),
                        0, 0
                    };
                    SDL_BlitSurface(progress_surface.get(), NULL, dest_surface, &progress_rect);
                }
            }
        }
    }

    return true;
}

bool GalleryView::is_done()
{
    return state->_is_done;
}

void GalleryView::on_keypress(SDLKey key)
{
    const auto &entries = state->browse.get_entries();
    uint32_t num_entries = static_cast<uint32_t>(entries.size());

    if (num_entries == 0)
    {
        if (key == SW_BTN_B)
        {
            state->_is_done = true;
        }
        return;
    }

    switch (key)
    {
        case SW_BTN_LEFT:
            if (state->cursor_pos > 0)
            {
                state->cursor_pos--;
                state->needs_render = true;
                notify_focus(state.get());
            }
            break;
        case SW_BTN_RIGHT:
            if (state->cursor_pos + 1 < num_entries)
            {
                state->cursor_pos++;
                state->needs_render = true;
                notify_focus(state.get());
            }
            break;
        case SW_BTN_UP:
            if (state->cursor_pos > 0)
            {
                state->cursor_pos = (state->cursor_pos >= TILE_COLUMNS) ? state->cursor_pos - TILE_COLUMNS : 0;
                state->needs_render = true;
                notify_focus(state.get());
            }
            break;
        case SW_BTN_DOWN:
            {
                uint32_t new_pos = std::min(state->cursor_pos + TILE_COLUMNS, num_entries - 1);
                if (new_pos != state->cursor_pos)
                {
                    state->cursor_pos = new_pos;
                    state->needs_render = true;
                    notify_focus(state.get());
                }
            }
            break;
        case SW_BTN_A:
            {
                auto result = state->browse.enter(state->cursor_pos);
                if (result.file_selected)
                {
                    if (state->on_file_selected)
                    {
                        state->on_file_selected(*result.file_selected);
                    }
                }
                else if (result.nav_action == BrowseNavAction::WentUp)
                {
                    state->cursor_pos = find_entry_index(state->browse.get_entries(), result.restore_name);
                    state->scroll_row = 0;
                    state->needs_render = true;
                    notify_focus(state.get());
                }
                else if (result.nav_action == BrowseNavAction::WentDown)
                {
                    const auto &new_entries = state->browse.get_entries();
                    state->cursor_pos = new_entries.size() > 1 ? 1 : 0; // get past ".." entry
                    state->scroll_row = 0;
                    state->needs_render = true;
                    notify_focus(state.get());
                }
            }
            break;
        case SW_BTN_B:
            state->_is_done = true;
            break;
        default:
            break;
    }
}

void GalleryView::on_keyheld(SDLKey key, uint32_t held_time_ms)
{
    switch (key)
    {
        case SW_BTN_UP:
        case SW_BTN_DOWN:
        case SW_BTN_LEFT:
        case SW_BTN_RIGHT:
            if (state->move_throttle(held_time_ms))
            {
                on_keypress(key);
            }
            break;
        default:
            break;
    }
}

void GalleryView::on_focus()
{
    if (state->on_view_focus)
    {
        state->on_view_focus();
    }
}

void GalleryView::set_on_file_selected(std::function<void(const std::filesystem::path &)> callback)
{
    state->on_file_selected = callback;
}

void GalleryView::set_on_file_focus(std::function<void(const std::filesystem::path &)> callback)
{
    state->on_file_focus = callback;
}

void GalleryView::set_on_view_focus(std::function<void()> callback)
{
    state->on_view_focus = callback;
}
