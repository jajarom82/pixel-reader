#include "./reader_view.h"

#include "./selection_menu.h"
#include "./token_view/token_view.h"
#include "./token_view/token_view_styling.h"

#include "reader/state_store.h"
#include "reader/system_styling.h"
#include "reader/view_stack.h"

#include "doc_api/doc_reader.h"
#include "doc_api/doc_token.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/sdl_font_cache.h"
#include "util/utf8.h"

#include <algorithm>
#include <cctype>
#include <iostream>

struct ReaderViewState
{
    bool is_done = false;

    std::function<void(DocAddr)> on_change_address;

    std::string filename;
    std::shared_ptr<DocReader> reader;
    SystemStyling &sys_styling;
    TokenViewStyling &token_view_styling;
    uint32_t token_view_styling_sub_id;

    ViewStack &view_stack;
    StateStore &state_store;

    std::unique_ptr<TokenView> token_view;

    ReaderViewState(std::filesystem::path path, DocAddr seek_address, std::shared_ptr<DocReader> reader, SystemStyling &sys_styling, TokenViewStyling &token_view_styling, uint32_t token_view_styling_sub_id, ViewStack &view_stack, StateStore &state_store)
        : filename(path.filename()),
          reader(reader),
          sys_styling(sys_styling),
          token_view_styling(token_view_styling),
          token_view_styling_sub_id(token_view_styling_sub_id),
          view_stack(view_stack),
          state_store(state_store),
          token_view(std::make_unique<TokenView>(
              reader,
              seek_address,
              sys_styling,
              token_view_styling
          ))
    {
    }

    ~ReaderViewState()
    {
    }
};

namespace
{

DocAddr get_current_address(const ReaderViewState &state)
{
    if (state.token_view)
    {
        return state.token_view->get_address();
    }

    return 0;
}

// A short, single-line preview of the book's own text at `address`, used
// as a bookmark's label so it is recognizable in a list without needing
// to jump to it first.
std::string get_bookmark_label(DocReader &reader, DocAddr address)
{
    auto iter = reader.get_iter(address);
    const DocToken *token = iter ? iter->read(1) : nullptr;

    std::string text;
    if (token)
    {
        switch (token->type)
        {
            case TokenType::Text:
                text = static_cast<const TextDocToken *>(token)->text;
                break;
            case TokenType::Header:
                text = static_cast<const HeaderDocToken *>(token)->text;
                break;
            case TokenType::ListItem:
                text = static_cast<const ListItemDocToken *>(token)->text;
                break;
            case TokenType::Image:
                text = "[Image]";
                break;
        }
    }

    // Collapse any embedded whitespace/newlines to single spaces, so a
    // multi-line paragraph still becomes one bookmark-list row.
    std::string collapsed;
    bool last_was_space = false;
    for (char c : text)
    {
        if (std::isspace(static_cast<unsigned char>(c)))
        {
            if (!last_was_space && !collapsed.empty())
            {
                collapsed += ' ';
            }
            last_was_space = true;
        }
        else
        {
            collapsed += c;
            last_was_space = false;
        }
    }

    constexpr size_t MAX_CHARS = 40;
    const char *pos = collapsed.c_str();
    const char *end = pos + collapsed.size();
    for (size_t i = 0; i < MAX_CHARS && pos < end; ++i)
    {
        pos = utf8_step(pos);
    }

    if (pos < end)
    {
        return collapsed.substr(0, pos - collapsed.c_str()) + "...";
    }
    return collapsed;
}

// A bookmark is considered to belong to the currently displayed page (not
// just the exact top-of-screen address) so toggling it feels like "mark
// this page" rather than requiring pixel-precise positioning.
const Bookmark *find_bookmark_on_current_page(const ReaderViewState &state)
{
    DocAddr top = state.token_view->get_address();
    DocAddr bottom = state.token_view->get_last_visible_address();

    for (const auto &bookmark : state.state_store.get_bookmarks(state.reader->get_id()))
    {
        if (bookmark.address >= top && bookmark.address <= bottom)
        {
            return &bookmark;
        }
    }
    return nullptr;
}

void toggle_bookmark_here(ReaderViewState &state)
{
    auto book_id = state.reader->get_id();

    if (const Bookmark *existing = find_bookmark_on_current_page(state))
    {
        state.state_store.remove_bookmark(book_id, existing->address);
    }
    else
    {
        DocAddr address = state.token_view->get_address();
        state.state_store.add_bookmark(book_id, address, get_bookmark_label(*state.reader, address));
    }
}

void open_toc_menu(ReaderView &reader_view, ReaderViewState &state)
{
    const auto &toc = state.reader->get_table_of_contents();
    const auto &bookmarks = state.state_store.get_bookmarks(state.reader->get_id());

    if (toc.empty() && bookmarks.empty())
    {
        return;
    }

    auto current_toc_index = state.reader->get_toc_position(get_current_address(state)).toc_index;

    std::vector<MenuEntry> menu_names;
    std::vector<DocAddr> menu_addresses;
    // Selecting the chapter the reader is already in would otherwise reset
    // their scroll position back to its start - skip seeking in that case.
    std::vector<bool> is_current_toc_entry;

    for (uint32_t i = 0; i < toc.size(); ++i)
    {
        std::string indent(toc[i].indent_level * 2, ' ');
        menu_names.push_back(indent + toc[i].display_name);
        menu_addresses.push_back(state.reader->get_toc_item_address(i));
        is_current_toc_entry.push_back(i == current_toc_index);
    }

    for (const auto &bookmark : bookmarks)
    {
        menu_names.push_back(std::string("★ ") + bookmark.label);
        menu_addresses.push_back(bookmark.address);
        is_current_toc_entry.push_back(false);
    }

    auto toc_select_menu = std::make_shared<SelectionMenu>(
        menu_names,
        state.sys_styling
    );
    toc_select_menu->set_on_selection([&reader_view, menu_addresses, is_current_toc_entry](uint32_t index) {
        if (index < menu_addresses.size() && !is_current_toc_entry[index])
        {
            reader_view.seek_to_address(menu_addresses[index]);
        }
    });
    toc_select_menu->set_close_on_select();

    // select current toc item
    if (current_toc_index < toc.size())
    {
        toc_select_menu->set_cursor_pos(current_toc_index);
    }

    toc_select_menu->set_default_on_keypress([](SDLKey key, SelectionMenu &toc) {
        if (key == SW_BTN_SELECT)
        {
            toc.close();
        }
    });

    state.view_stack.push(toc_select_menu);
}

} // namespace

ReaderView::ReaderView(
    std::filesystem::path path,
    std::shared_ptr<DocReader> reader,
    DocAddr seek_address,
    SystemStyling &sys_styling,
    TokenViewStyling &token_view_styling,
    ViewStack &view_stack,
    StateStore &state_store
) : state(std::make_unique<ReaderViewState>(
        path,
        seek_address,
        reader,
        sys_styling,
        token_view_styling,
        token_view_styling.subscribe_to_changes([this]() {
            update_token_view_title(get_current_address(*state));
        }),
        view_stack,
        state_store
    ))
{
    update_token_view_title(seek_address);

    // Update title info on scroll
    state->token_view->set_on_scroll([this](DocAddr address) {
        update_token_view_title(address);

        if (state->on_change_address)
        {
            state->on_change_address(address);
        }
    });
}

ReaderView::~ReaderView()
{
    state->token_view_styling.unsubscribe_from_changes(
        state->token_view_styling_sub_id
    );
}

void ReaderView::update_token_view_title(DocAddr address)
{
    const auto &toc = state->reader->get_table_of_contents();
    auto toc_position = state->reader->get_toc_position(address);

    std::string title = (
        toc_position.toc_index < toc.size() ?
        toc[toc_position.toc_index].display_name :
        state->filename
    );

    if (find_bookmark_on_current_page(*state))
    {
        title = "★ " + title;
    }

    state->token_view->set_title(title);

    uint32_t progress_percent = (
        state->token_view_styling.get_progress_reporting() == ProgressReporting::CHAPTER_PERCENT ?
        toc_position.progress_percent :
        state->reader->get_global_progress_percent(address)
    );
    state->token_view->set_title_progress(progress_percent);
}

bool ReaderView::render(SDL_Surface *dest_surface, bool force_render)
{
    return state->token_view->render(dest_surface, force_render);
}

bool ReaderView::is_done()
{
    return state->is_done;
}

void ReaderView::on_keypress(SDLKey key)
{
    if (key == SW_BTN_B)
    {
        state->is_done = true;
        return;
    }

    switch (key) {
        case SW_BTN_A:
            state->token_view_styling.set_show_title_bar(
                !state->token_view_styling.get_show_title_bar()
            );
            break;
        case SW_BTN_SELECT:
            open_toc_menu(*this, *state);
            break;
        case SW_BTN_START:
            toggle_bookmark_here(*state);
            update_token_view_title(get_current_address(*state));
            break;
        default:
            state->token_view->on_keypress(key);
            break;
    }
}

void ReaderView::on_keyheld(SDLKey key, uint32_t hold_time_ms)
{
    state->token_view->on_keyheld(key, hold_time_ms);
}

void ReaderView::on_tick(uint32_t elapsed_ms)
{
    state->token_view->on_tick(elapsed_ms);
}

bool ReaderView::wants_continuous_render() const
{
    return state->token_view->wants_continuous_render();
}

void ReaderView::set_on_change_address(std::function<void(DocAddr)> callback)
{
    state->on_change_address = callback;
}

void ReaderView::seek_to_toc_index(uint32_t toc_index)
{
    auto address = state->reader->get_toc_item_address(toc_index);
    seek_to_address(address);
}

void ReaderView::seek_to_address(DocAddr address)
{
    if (state->token_view)
    {
        state->token_view->seek_to_address(address);
        update_token_view_title(address);

        if (state->on_change_address)
        {
            state->on_change_address(address);
        }
    }
}
