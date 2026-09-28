#ifndef BROWSE_STATE_H_
#define BROWSE_STATE_H_

#include "sys/filesystem.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

enum class BrowseNavAction
{
    None,
    WentDown,
    WentUp,
};

struct BrowseEnterResult
{
    std::optional<std::filesystem::path> file_selected;
    BrowseNavAction nav_action = BrowseNavAction::None;
    std::string restore_name; // only meaningful when nav_action == WentUp
};

// Directory-navigation state machine shared by the list (FileSelector) and
// gallery (GalleryView) browse UIs, so both behave identically (".."
// handling, supported-extension filtering, starting-path sanitization)
// instead of maintaining two separate implementations. Knows nothing about
// how entries are displayed/highlighted/cached - that's left to the
// UI-specific view.
class BrowseState
{
    std::filesystem::path path;
    std::vector<FSEntry> entries;

    void refresh();

public:
    explicit BrowseState(std::filesystem::path starting_path);

    const std::filesystem::path &get_path() const;
    const std::vector<FSEntry> &get_entries() const;

    // `index` into get_entries(). For a directory, navigates (mutating
    // get_path()/get_entries()) and reports how the cursor should move.
    // For a file, just reports it back - caller decides whether to open it.
    BrowseEnterResult enter(uint32_t index);
};

#endif
