#ifndef GALLERY_VIEW_H_
#define GALLERY_VIEW_H_

#include "reader/view.h"

#include <SDL/SDL_video.h>

#include <functional>
#include <filesystem>
#include <memory>
#include <string>

struct GVState;
struct StateStore;
struct SystemStyling;

// Grid/cover browse UI - an alternative to FileSelector's list view, driven
// by the same BrowseState navigation rules so the two behave identically
// (".." handling, supported-extension filtering, cursor memory going up a
// directory). Same public interface as FileSelector so main.cpp can
// construct either one interchangeably based on the browse-view setting.
class GalleryView: public View
{
    std::unique_ptr<GVState> state;

public:
    // Expects to receive a path to a file, or directory with trailing separator.
    GalleryView(std::filesystem::path path, SystemStyling &styling, StateStore &state_store);
    virtual ~GalleryView();

    bool render(SDL_Surface *dest_surface, bool force_render) override;
    bool is_done() override;
    void on_keypress(SDLKey key) override;
    void on_keyheld(SDLKey key, uint32_t held_time_ms) override;
    void on_focus() override;

    void set_on_file_selected(std::function<void(const std::filesystem::path &)> on_file_selected);
    void set_on_file_focus(std::function<void(const std::filesystem::path &)> on_file_focus);
    void set_on_view_focus(std::function<void()> on_view_focus);
};

#endif
