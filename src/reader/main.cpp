#include "./battery_status.h"
#include "./config.h"
#include "./font_catalog.h"
#include "./rotation.h"
#include "./settings_store.h"
#include "./shoulder_keymap.h"
#include "./state_store.h"
#include "./system_styling.h"
#include "./color_theme_def.h"
#include "./view_stack.h"
#include "./views/file_selector.h"
#include "./views/gallery_view.h"
#include "./views/reader_bootstrap_view.h"
#include "./views/reader_view.h"
#include "./views/settings_view.h"
#include "./views/token_view/token_view_styling.h"
#include "filetypes/open_doc.h"
#include "sys/battery.h"
#include "sys/keymap.h"
#include "sys/screen.h"
#include "util/fps_limiter.h"
#include "util/held_key_tracker.h"
#include "util/key_value_file.h"
#include "util/math.h"
#include "util/rotate_blit.h"
#include "util/sdl_font_cache.h"
#include "util/sdl_pointer.h"
#include "util/task_queue.h"
#include "util/timer.h"

#include <libxml/parser.h>
#include <SDL/SDL.h>

#include <cstdint>
#include <csignal>
#include <iostream>

namespace
{

void initialize_views(
    ViewStack &view_stack,
    StateStore &state_store,
    SystemStyling &sys_styling,
    TokenViewStyling &token_view_styling,
    TaskQueue &task_queue,
    std::optional<std::filesystem::path> requested_book_path
)
{
    auto load_book = [&view_stack, &state_store, &sys_styling, &token_view_styling, &task_queue](std::filesystem::path path) {
        if (!std::filesystem::exists(path))
        {
            std::cerr << path << " does not exist" << std::endl;
            return;
        }
        if (!file_type_is_supported(path))
        {
            std::cerr << path << " filetype is not supported" << std::endl;
            return;
        }

        view_stack.push(
            std::make_shared<ReaderBootstrapView>(
                path,
                sys_styling,
                token_view_styling,
                view_stack,
                state_store,
                [&task_queue](task_func task){ task_queue.submit(task); }
            )
        );
    };

    if (requested_book_path)
    {
        load_book(*requested_book_path);
    }
    else
    {
        auto browse_path = state_store.get_current_browse_path().value_or(DEFAULT_BROWSE_PATH);
        bool use_gallery = settings_get_browse_view_mode(state_store).value_or(DEFAULT_BROWSE_VIEW_MODE) == "gallery";

        auto wire_browser = [&](auto &browser) {
            browser->set_on_file_selected(load_book);
            browser->set_on_file_focus([&state_store](const std::filesystem::path &path) {
                state_store.set_current_browse_path(path);
            });
            browser->set_on_view_focus([&state_store]() {
                state_store.remove_current_book_path();
            });
        };

        std::shared_ptr<View> fs;
        if (use_gallery)
        {
            auto gallery = std::make_shared<GalleryView>(browse_path, sys_styling, state_store);
            wire_browser(gallery);
            fs = gallery;
        }
        else
        {
            auto list_view = std::make_shared<FileSelector>(browse_path, sys_styling, state_store);
            wire_browser(list_view);
            fs = list_view;
        }

        view_stack.push(fs);

        if (state_store.get_current_book_path())
        {
            load_book(state_store.get_current_book_path().value());
        }
    }
}

class SystemKeyChordTracker
{
    bool _menu_held = false;
    bool _select_held = false;

    bool _exit_on_menu_release = false;
    bool _exit_requested = false;

public:

    // Report keypress event. Return filtered key code.
    SDLKey on_keypress(SDLKey key)
    {
        // Block any other keys while special key is held
        SDLKey filtered_key = (_menu_held || _select_held) ? SDLK_UNKNOWN : key;

        if (key == SW_BTN_SELECT)
        {
            _select_held = true;
        }

        if (key == SW_BTN_MENU)
        {
            _menu_held = true;
            _exit_on_menu_release = true;
        }
        else
        {
            // Cancel app exit if a menu chord was used (e.g. change brightness)
            _exit_on_menu_release = false;
        }

        return filtered_key;
    }

    // Report keyrelease event.
    void on_keyrelease(SDLKey key)
    {
        if (key == SW_BTN_MENU)
        {
            _menu_held = false;

            if (_exit_on_menu_release)
            {
                _exit_requested = true;
            }
        }
        else if (key == SW_BTN_SELECT)
        {
            _select_held = false;
        }
    }

    bool exit_requested() const
    {
        return _exit_requested;
    }
};

// Portable substitute for SDL_WaitEventTimeout, which isn't declared in
// every SDL1.2 build (notably absent from the Miyoo Mini cross-compile
// toolchain's headers). Polls in a short sleep loop instead of a single
// blocking call - still lets the CPU idle between polls, just with
// POLL_STEP_MS granularity instead of an instant wake. 50ms caps added
// input latency well under what's perceptible for turning a page or
// waking from idle, while waking the CPU 5x less often than a more
// cautious 10ms step would while nothing is happening.
bool poll_event_with_timeout(SDL_Event *event, uint32_t timeout_ms)
{
    constexpr uint32_t POLL_STEP_MS = 50;
    uint32_t start = SDL_GetTicks();
    while (true)
    {
        if (SDL_PollEvent(event))
        {
            return true;
        }
        if (SDL_GetTicks() - start >= timeout_ms)
        {
            return false;
        }
        SDL_Delay(POLL_STEP_MS);
    }
}

bool quit = false;

void signal_handler(int)
{
    quit = true;
}

const char *CONFIG_KEY_STORE_PATH = "store_path";

std::unordered_map<std::string, std::string> load_config_with_defaults()
{
    auto config = load_key_value(CONFIG_FILE_PATH);
    config.try_emplace(CONFIG_KEY_STORE_PATH, FALLBACK_STORE_PATH);
    return config;
}

} // namespace

int main(int argc, char **argv)
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    unsigned int physical_width = SCREEN_WIDTH;
    unsigned int physical_height = SCREEN_HEIGHT;

    if (char* env_screen_width = SDL_getenv("SCREEN_WIDTH")) {
        int new_width = atoi(env_screen_width);
        if (100 < new_width && new_width < 4096)
            physical_width = static_cast<unsigned int>(new_width);
    }

    if (char* env_screen_height = SDL_getenv("SCREEN_HEIGHT")) {
        int new_height = atoi(env_screen_height);
        if (100 < new_height && new_height < 4096)
            physical_height = static_cast<unsigned int>(new_height);
    }

    // SDL Init
    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(SDL_DISABLE);
    TTF_Init();

    auto config = load_config_with_defaults();
    StateStore state_store(config[CONFIG_KEY_STORE_PATH]);

    // Rotation must be known before creating the surfaces below, since a
    // 90/270 rotation swaps the logical (view-facing) screen dimensions
    // relative to the physical panel.
    std::string rotation = get_valid_rotation(settings_get_rotation(state_store).value_or(DEFAULT_ROTATION));

    auto is_rotation_swapped = [](const std::string &r) {
        int degrees = get_rotation_degrees(r);
        return degrees == 90 || degrees == 270;
    };

    SCREEN_WIDTH = is_rotation_swapped(rotation) ? physical_height : physical_width;
    SCREEN_HEIGHT = is_rotation_swapped(rotation) ? physical_width : physical_height;

    // launch.sh redirects only stderr to log.txt (`./reader 2>log.txt`), not
    // stdout - anything meant to be checkable after a normal launch from the
    // device's menu (no terminal attached) must go to std::cerr, not
    // std::cout, or it goes nowhere anyone can read.
    std::cerr << "Physical screen size: " << physical_width << "x" << physical_height << std::endl;
    std::cerr << "Logical screen size: " << SCREEN_WIDTH << "x" << SCREEN_HEIGHT << std::endl;

    // Surfaces
    SDL_Surface *video = SDL_SetVideoMode(physical_width, physical_height, 32, SDL_HWSURFACE);
    SDL_Surface *screen = SDL_CreateRGBSurface(SDL_HWSURFACE, SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0, 0, 0, 0);
    set_render_surface_format(screen->format);

    // Scratch buffer content is copied or rotated into before reaching the
    // physical panel - always allocated and always used, even for a plain
    // (0 degree) pass-through. On-device pixel readback proved that this
    // device's blit-to-video path applies a hidden, unwanted 180 degree
    // flip specifically when the source is `screen` (a hardware surface),
    // but never when the source is this software-surface scratch buffer -
    // so `screen` must never be blitted directly to `video`, regardless of
    // whether a rotation is actually needed. Its size is always
    // physical_width x physical_height regardless of *which* rotation is
    // active (0/90/180/270 all land on the same physical panel shape).
    SDL_Surface *rotated_buffer = SDL_CreateRGBSurface(SDL_SWSURFACE, physical_width, physical_height, 32, 0, 0, 0, 0);
    if (!rotated_buffer)
    {
        std::cerr << "Failed to allocate rotation buffer" << std::endl;
    }

    // Custom theme colors must be populated before SystemStyling is
    // constructed, in case the persisted color theme choice is "custom".
    set_custom_theme_colors(
        settings_get_custom_background_color(state_store).value_or(get_custom_background_color()),
        settings_get_custom_main_text_color(state_store).value_or(get_custom_main_text_color())
    );

    // Preload & check fonts
    auto init_font_name = get_valid_font_name(settings_get_font_name(state_store).value_or(DEFAULT_FONT_NAME));
    auto init_font_size = bound(settings_get_font_size(state_store).value_or(DEFAULT_FONT_SIZE), MIN_FONT_SIZE, MAX_FONT_SIZE);
    if (
        !cached_load_font(SYSTEM_FONT, init_font_size, FontLoadErrorOpt::NoThrow) ||
        !cached_load_font(init_font_name, init_font_size, FontLoadErrorOpt::NoThrow)
    )
    {
        std::cerr << "Failed to load one or more fonts" << std::endl;
        return 1;
    }

    // System styling
    SystemStyling sys_styling(
        init_font_name,
        init_font_size,
        get_valid_theme(settings_get_color_theme(state_store).value_or(DEFAULT_COLOR_THEME)),
        get_valid_shoulder_keymap(settings_get_shoulder_keymap(state_store).value_or(DEFAULT_SHOULDER_KEYMAP)),
        rotation
    );
    sys_styling.subscribe_to_changes([&](SystemStyling::ChangeId change_id) {
        // Persist changes
        settings_set_color_theme(state_store, sys_styling.get_color_theme());
        settings_set_font_name(state_store, sys_styling.get_font_name());
        settings_set_font_size(state_store, sys_styling.get_font_size());
        settings_set_shoulder_keymap(state_store, sys_styling.get_shoulder_keymap());
        settings_set_rotation(state_store, sys_styling.get_rotation());
        settings_set_custom_background_color(state_store, sys_styling.get_custom_background_color());
        settings_set_custom_main_text_color(state_store, sys_styling.get_custom_main_text_color());

        if (change_id == SystemStyling::ChangeId::ROTATION)
        {
            // Take effect immediately: resize the logical surface (views
            // read SCREEN_WIDTH/HEIGHT live) and the rotation scratch
            // buffer. The physical video surface/mode is unchanged.
            rotation = sys_styling.get_rotation();

            SCREEN_WIDTH = is_rotation_swapped(rotation) ? physical_height : physical_width;
            SCREEN_HEIGHT = is_rotation_swapped(rotation) ? physical_width : physical_height;

            SDL_FreeSurface(screen);
            screen = SDL_CreateRGBSurface(SDL_HWSURFACE, SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0, 0, 0, 0);
            set_render_surface_format(screen->format);

            std::cerr << "Rotation changed to " << rotation
                << " (" << get_rotation_degrees(rotation) << " degrees), logical "
                << SCREEN_WIDTH << "x" << SCREEN_HEIGHT
                << std::endl;
        }
    });

    // Text Styling
    TokenViewStyling token_view_styling(
        settings_get_show_title_bar(state_store).value_or(DEFAULT_SHOW_PROGRESS),
        settings_get_progress_reporting(state_store).value_or(DEFAULT_PROGRESS_REPORTING),
        settings_get_auto_scroll_speed(state_store).value_or(DEFAULT_AUTO_SCROLL_SPEED)
    );
    token_view_styling.subscribe_to_changes([&token_view_styling, &state_store]() {
        // Persist changes
        settings_set_show_title_bar(state_store, token_view_styling.get_show_title_bar());
        settings_set_progress_reporting(state_store, token_view_styling.get_progress_reporting());
        settings_set_auto_scroll_speed(state_store, token_view_styling.get_auto_scroll_speed());
    });

    // Setup views
    TaskQueue task_queue;
    ViewStack view_stack;

    std::optional<std::filesystem::path> requested_book_path = (
        argc == 2 ? std::optional<std::filesystem::path>(argv[1]) : std::nullopt
    );
    initialize_views(
        view_stack,
        state_store,
        sys_styling,
        token_view_styling,
        task_queue,
        requested_book_path
    );
    quit = view_stack.is_done();

    std::shared_ptr<SettingsView> settings_view = std::make_shared<SettingsView>(
        sys_styling,
        token_view_styling,
        state_store,
        SYSTEM_FONT
    );

    // Track held keys
    HeldKeyTracker held_key_tracker(
        {
            SW_BTN_UP,
            SW_BTN_DOWN,
            SW_BTN_LEFT,
            SW_BTN_RIGHT,
            SW_BTN_L1,
            SW_BTN_R1,
            SW_BTN_L2,
            SW_BTN_R2
        }
    );
    SystemKeyChordTracker chord_tracker;

    auto key_held_callback = [&view_stack](SDLKey key, uint32_t held_ms) {
        view_stack.on_keyheld(key, held_ms);
    };

    // Timing
    Timer idle_timer;
    Timer tick_timer;
    FPSLimiter limit_fps(TARGET_FPS);
    const uint32_t avg_loop_time = 1000 / TARGET_FPS;

    // Present the logical `screen` surface to the physical `video` surface,
    // always by way of `rotated_buffer` - a straight copy for a 0 degree
    // result, otherwise rotated first. `screen` (a hardware surface) must
    // never be blitted directly to `video`: on this device that specific
    // combination silently applies an unwanted 180 degree flip that a
    // software-surface source never triggers.
    //
    // Whatever rotation the user asked for is also applied 180 degrees
    // opposite of what its label says (0 actually rotates 180, 180 applies
    // no rotation, 90/270 swap) - confirmed on real hardware to be needed
    // for the display to end up right side up, most likely because the
    // physical panel itself is mounted rotated relative to the
    // framebuffer's natural orientation.
    auto present = [&]() {
        int degrees_to_apply = (get_rotation_degrees(rotation) + 180) % 360;
        if (degrees_to_apply == 0)
        {
            SDL_BlitSurface(screen, NULL, rotated_buffer, NULL);
        }
        else
        {
            rotate_blit(screen, rotated_buffer, degrees_to_apply);
        }
        SDL_BlitSurface(rotated_buffer, NULL, video, NULL);
        SDL_Flip(video);
    };

    // Battery indicator, drawn as a small overlay directly onto `screen`
    // (whatever it currently holds) so it can refresh on its own poll
    // cadence without requiring a full view re-render. Framed as a small
    // battery-shaped icon (bordered body + terminal nub) with the
    // percentage centered inside and a thin proportional charge gauge
    // along the bottom edge, rather than bare text.
    BatteryStatus battery_status;
    auto draw_battery_overlay = [&]() {
        auto percent = battery_status.get_percent();
        if (!percent)
        {
            return;
        }

        TTF_Font *font = cached_load_font(SYSTEM_FONT, sys_styling.get_font_size());
        const auto &theme = sys_styling.get_loaded_color_theme();

        const int margin = 5;
        const int h_pad = 4;
        const int border = 1;
        const int gauge_h = 3;
        const int gauge_gap = 2;
        const int nub_w = 3;
        const int nub_gap = 1;

        // Sized for the widest case ("100%") so a shrinking percentage
        // (e.g. 100% -> 99%) can't leave stale digits from a wider
        // previous frame.
        int text_w = 0, text_h = 0;
        TTF_SizeUTF8(font, "100%", &text_w, &text_h);

        int body_w = text_w + h_pad * 2;
        int body_h = text_h + gauge_gap + gauge_h;
        int total_w = body_w + nub_gap + nub_w;

        SDL_Rect zone_rect = {
            static_cast<Sint16>(SCREEN_WIDTH - total_w - margin),
            margin,
            static_cast<Uint16>(total_w),
            static_cast<Uint16>(body_h)
        };
        SDL_FillRect(
            screen,
            &zone_rect,
            SDL_MapRGB(screen->format, theme.background.r, theme.background.g, theme.background.b)
        );

        auto to_mapped = [&](const SDL_Color &c) {
            return SDL_MapRGB(screen->format, c.r, c.g, c.b);
        };

        SDL_Rect body_rect = {
            zone_rect.x, zone_rect.y,
            static_cast<Uint16>(body_w), static_cast<Uint16>(body_h)
        };
        SDL_FillRect(screen, &body_rect, to_mapped(theme.secondary_text));

        SDL_Rect body_inner_rect = {
            static_cast<Sint16>(body_rect.x + border),
            static_cast<Sint16>(body_rect.y + border),
            static_cast<Uint16>(body_w - border * 2),
            static_cast<Uint16>(body_h - border * 2)
        };
        SDL_FillRect(screen, &body_inner_rect, to_mapped(theme.background));

        SDL_Rect nub_rect = {
            static_cast<Sint16>(body_rect.x + body_w + nub_gap),
            static_cast<Sint16>(body_rect.y + body_h / 4),
            static_cast<Uint16>(nub_w),
            static_cast<Uint16>(body_h / 2)
        };
        SDL_FillRect(screen, &nub_rect, to_mapped(theme.secondary_text));

        // Charge gauge along the bottom edge, switching to a fixed warning
        // color when low regardless of theme - the same convention as most
        // OS battery indicators, where legibility of "running low" matters
        // more than matching the current color scheme.
        int gauge_max_w = body_inner_rect.w - 2;
        int gauge_w = gauge_max_w * bound(*percent, 0, 100) / 100;
        if (gauge_w > 0)
        {
            bool low_battery = *percent <= 15;
            SDL_Color gauge_color = low_battery ? SDL_Color{230, 70, 70, 0} : theme.highlight_background;

            SDL_Rect gauge_rect = {
                static_cast<Sint16>(body_inner_rect.x + 1),
                static_cast<Sint16>(body_inner_rect.y + body_inner_rect.h - gauge_h - 1),
                static_cast<Uint16>(gauge_w),
                static_cast<Uint16>(gauge_h)
            };
            SDL_FillRect(screen, &gauge_rect, to_mapped(gauge_color));
        }

        char text[16];
        snprintf(text, sizeof(text), "%d%%", *percent);

        surface_unique_ptr surface { TTF_RenderUTF8_Shaded(font, text, theme.secondary_text, theme.background) };
        if (!surface)
        {
            return;
        }

        SDL_Rect dest_rect = {
            static_cast<Sint16>(body_inner_rect.x + (body_inner_rect.w - surface->w) / 2),
            body_inner_rect.y,
            0, 0
        };
        SDL_BlitSurface(surface.get(), NULL, screen, &dest_rect);
    };

    // Initial render
    view_stack.render(screen, true);
    draw_battery_overlay();
    present();

    while (!quit)
    {
        bool ran_user_code = task_queue.drain();

        {
            uint32_t elapsed_ms = tick_timer.elapsed_ms();
            tick_timer.reset();
            view_stack.on_tick(elapsed_ms);
        }

        bool battery_changed = battery_status.poll();

        // Only stay on a tight poll+sleep cadence while something is held or
        // animating; otherwise block until the next real event to let the
        // CPU idle between key presses.
        bool need_fast_ticks = held_key_tracker.any_held() || (
            view_stack.top_view() && view_stack.top_view()->wants_continuous_render()
        );

        SDL_Event event;
        bool got_event = (!ran_user_code && !need_fast_ticks)
            ? poll_event_with_timeout(&event, IDLE_POLL_TIMEOUT_MS)
            : SDL_PollEvent(&event) != 0;

        while (got_event)
        {
            switch (event.type)
            {
                case SDL_QUIT:
                    quit = true;
                    break;
                case SDL_KEYDOWN:
                    {
                        idle_timer.reset();

                        SDLKey key = chord_tracker.on_keypress(event.key.keysym.sym);

                        if (key == SW_BTN_POWER)
                        {
                            state_store.flush();
                        }
                        else
                        {
                            view_stack.on_keypress(key);

                            if (key == SW_BTN_X)
                            {
                                if (view_stack.top_view() != settings_view)
                                {
                                    settings_view->unterminate();
                                    view_stack.push(settings_view);
                                }
                                else
                                {
                                    settings_view->terminate();
                                }
                            }

                            ran_user_code = true;
                        }
                    }
                    break;
                case SDL_KEYUP:
                    {
                        SDLKey key = event.key.keysym.sym;
                        chord_tracker.on_keyrelease(key);
                    }
                    break;
                default:
                    break;
            }

            got_event = SDL_PollEvent(&event) != 0;
        }

        quit = quit || chord_tracker.exit_requested();

        held_key_tracker.accumulate(avg_loop_time); // Pretend perfect loop timing for event firing consistency
        ran_user_code = held_key_tracker.for_longest_held(key_held_callback) || ran_user_code;
        ran_user_code = ran_user_code || need_fast_ticks;

        bool view_rendered = false;
        if (ran_user_code)
        {
            bool force_render = view_stack.pop_completed_views();

            if (view_stack.is_done())
            {
                quit = true;
            }

            view_rendered = view_stack.render(screen, force_render);
        }

        // Hidden while actively reading - a persistent corner indicator
        // competes with the page for attention exactly where it matters
        // least to have it. Still shown while browsing/in settings/TOC.
        bool show_battery = dynamic_cast<ReaderView *>(view_stack.top_view().get()) == nullptr;
        bool relevant_battery_change = battery_changed && show_battery;

        if (view_rendered || relevant_battery_change)
        {
            // Battery-only updates don't need (and shouldn't force) a full
            // view re-render: `screen` already holds the last frame's
            // contents, so just redraw the indicator on top of it.
            if (show_battery)
            {
                draw_battery_overlay();
            }
            present();
        }

        if (!quit && need_fast_ticks)
        {
            limit_fps();
        }

        if (idle_timer.elapsed_sec() >= IDLE_SAVE_TIME_SEC)
        {
            // Make sure state is saved in case device auto-powers down. Don't seem
            // to get a signal on miyoo mini when this happens.
            state_store.flush();
            idle_timer.reset();
        }
    }

    view_stack.shutdown();
    state_store.flush();

    SDL_FreeSurface(screen);
    if (rotated_buffer)
    {
        SDL_FreeSurface(rotated_buffer);
    }
    SDL_Quit();
    xmlCleanupParser();
    
    return 0;
}
