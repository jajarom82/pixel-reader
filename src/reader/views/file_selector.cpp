#include "./file_selector.h"

#include "./browse_state.h"
#include "./selection_menu.h"
#include "reader/state_store.h"
#include "reader/system_styling.h"
#include "sys/filesystem.h"

#include <filesystem>
#include <vector>

struct FSState
{
    BrowseState browse;
    std::function<void(const std::filesystem::path &)> on_file_selected;
    std::function<void(const std::filesystem::path &)> on_file_focus;
    std::function<void()> on_view_focus;

    StateStore &state_store;
    SelectionMenu menu;

    FSState(std::filesystem::path path, SystemStyling &styling, StateStore &state_store)
        : browse(std::move(path)),
          state_store(state_store),
          menu(styling)
    {
    }
};

namespace {

std::vector<MenuEntry> build_menu_entries(FSState *s)
{
    std::vector<MenuEntry> menu_entries;
    for (const auto &entry : s->browse.get_entries())
    {
        std::string right_label;
        if (!entry.is_dir)
        {
            // Cheap: only a couple of key/value lookups, no need to open
            // the book to know how far into it we got last time.
            auto book_id = s->state_store.get_book_id_for_path(s->browse.get_path() / entry.name);
            if (book_id)
            {
                auto progress = s->state_store.get_book_progress(*book_id);
                if (progress && *progress > 0)
                {
                    right_label = std::to_string(*progress) + "%";
                }
            }
        }
        menu_entries.push_back(MenuEntry(entry.name, entry.is_dir, right_label));
    }
    return menu_entries;
}

void refresh_menu(FSState *s)
{
    s->menu.set_entries(build_menu_entries(s));
}

void on_menu_entry_selected(FSState *s, uint32_t menu_index)
{
    auto result = s->browse.enter(menu_index);

    if (result.file_selected)
    {
        if (s->on_file_selected)
        {
            s->on_file_selected(*result.file_selected);
        }
    }
    else if (result.nav_action == BrowseNavAction::WentUp)
    {
        refresh_menu(s);
        s->menu.set_cursor_pos(result.restore_name);
    }
    else if (result.nav_action == BrowseNavAction::WentDown)
    {
        refresh_menu(s);
        s->menu.set_cursor_pos(1); // get past ".." entry
    }
}

void on_menu_entry_focused(FSState *s, uint32_t menu_index)
{
    const auto &entries = s->browse.get_entries();
    if (!entries.empty() && s->on_file_focus && menu_index < entries.size())
    {
        s->on_file_focus(s->browse.get_path() / entries[menu_index].name);
    }
}

} // namespace

FileSelector::FileSelector(std::filesystem::path path, SystemStyling &styling, StateStore &state_store)
    : state(std::make_unique<FSState>(path, styling, state_store))
{
    state->menu.set_on_selection([this](uint32_t menu_index) {
        on_menu_entry_selected(this->state.get(), menu_index);
    });

    state->menu.set_on_focus([this](uint32_t menu_index) {
        on_menu_entry_focused(this->state.get(), menu_index);
    });

    refresh_menu(state.get());
    if (path.has_filename())
    {
        state->menu.set_cursor_pos(path.filename());
    }
    else
    {
        state->menu.set_cursor_pos(1); // get past ".." entry
    }
}

FileSelector::~FileSelector()
{
}

bool FileSelector::render(SDL_Surface *dest_surface, bool force_render)
{
    return state->menu.render(dest_surface, force_render);
}

bool FileSelector::is_done()
{
    return state->menu.is_done();
}

void FileSelector::on_keypress(SDLKey key)
{
    state->menu.on_keypress(key);
}

void FileSelector::on_keyheld(SDLKey key, uint32_t held_time_ms)
{
    state->menu.on_keyheld(key, held_time_ms);
}

void FileSelector::on_tick(uint32_t elapsed_ms)
{
    state->menu.on_tick(elapsed_ms);
}

bool FileSelector::wants_continuous_render() const
{
    return state->menu.wants_continuous_render();
}

void FileSelector::on_focus()
{
    if (state->on_view_focus)
    {
        state->on_view_focus();
    }
}

void FileSelector::set_on_file_selected(std::function<void(const std::filesystem::path &)> callback)
{
    state->on_file_selected = callback;
}

void FileSelector::set_on_file_focus(std::function<void(const std::filesystem::path &)> callback)
{
    state->on_file_focus = callback;
}

void FileSelector::set_on_view_focus(std::function<void()> callback)
{
    state->on_view_focus = callback;
}
