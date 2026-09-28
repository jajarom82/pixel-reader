#include "./browse_state.h"

#include "filetypes/open_doc.h"

#include <iostream>

namespace
{

std::filesystem::path sanitize_starting_path(std::filesystem::path path)
{
    path = std::filesystem::absolute(path);

    if (path.has_parent_path())
    {
        // get the directory component
        path = path.parent_path();
    }

    // make sure path exists
    while (!std::filesystem::is_directory(path))
    {
        std::cerr << "Directory " << path << " does not exist" << std::endl;
        if (path.has_parent_path() && path != path.root_path())
        {
            path = path.parent_path();
        }
        else
        {
            path = std::filesystem::current_path();
            break;
        }
    }

    return path;
}

} // namespace

void BrowseState::refresh()
{
    entries.clear();
    if (path.has_parent_path() && path != path.root_path())
    {
        entries.push_back(FSEntry::directory(".."));
    }

    for (const auto &entry : directory_listing(path))
    {
        if (entry.is_dir || file_type_is_supported(entry.name))
        {
            entries.push_back(entry);
        }
    }
}

BrowseState::BrowseState(std::filesystem::path starting_path)
    : path(sanitize_starting_path(std::move(starting_path)))
{
    refresh();
}

const std::filesystem::path &BrowseState::get_path() const
{
    return path;
}

const std::vector<FSEntry> &BrowseState::get_entries() const
{
    return entries;
}

BrowseEnterResult BrowseState::enter(uint32_t index)
{
    if (index >= entries.size())
    {
        return {};
    }

    const FSEntry &entry = entries[index];
    if (!entry.is_dir)
    {
        return { path / entry.name, BrowseNavAction::None, {} };
    }

    if (entry.name == "..")
    {
        std::string restore_name = path.filename();
        path = path.parent_path();
        refresh();
        return { std::nullopt, BrowseNavAction::WentUp, restore_name };
    }

    path /= entry.name;
    refresh();
    return { std::nullopt, BrowseNavAction::WentDown, {} };
}
