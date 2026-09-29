#include "./state_store.h"
#include "util/key_value_file.h"
#include "util/string_serialization.h"

#include <algorithm>
#include <fstream>
#include <unordered_map>

namespace
{

constexpr const char *ACTIVITY_KEY_BROWSER_PATH = "browser_path";
constexpr const char *ACTIVITY_KEY_BOOK_PATH = "book_path";
constexpr const char *ADDRESS_KEY = "address";
constexpr const char *PROGRESS_KEY = "progress";

/////////////////////////////////////
// Activity Store

void write_activity_store(const std::filesystem::path &path, const StateStore &store)
{
    string_unordered_map kv;

    const auto &browse_path = store.get_current_browse_path();
    if (browse_path)
    {
        kv[ACTIVITY_KEY_BROWSER_PATH] = browse_path.value().string();
    }

    const auto &book_path = store.get_current_book_path();
    if (book_path)
    {
        kv[ACTIVITY_KEY_BOOK_PATH] = book_path.value().string();
    }

    write_key_value(path, kv);
}

std::pair<std::optional<std::string>, std::optional<std::string>> load_activity_store(const std::filesystem::path &path)
{
    std::optional<std::string> browse_path, book_path;

    auto kv = load_key_value(path);

    auto browse_it = kv.find(ACTIVITY_KEY_BROWSER_PATH);
    if (browse_it != kv.end())
    {
        browse_path = browse_it->second;
    }

    auto book_it = kv.find(ACTIVITY_KEY_BOOK_PATH);
    if (book_it != kv.end())
    {
        book_path = book_it->second;
    }

    return {browse_path, book_path};
}

/////////////////////////////////////
// Address Store

std::filesystem::path address_store_path_for_book(const std::filesystem::path &base_path, const std::string &book_id)
{
    return base_path / (book_id + ".address");
}

void write_book_address(const std::filesystem::path &path, const DocAddr &address)
{
    string_unordered_map kv = {
        {ADDRESS_KEY, encode_address(address)}
    };

    write_key_value(path, kv);
}

std::optional<DocAddr> load_book_address(const std::filesystem::path &path)
{
    auto kv = load_key_value(path);
    auto it = kv.find(ADDRESS_KEY);
    if (it == kv.end())
    {
        return std::nullopt;
    }
    return decode_address(it->second);
}

/////////////////////////////////////
// Reader Cache Store

std::filesystem::path reader_cache_store_path_for_book(const std::filesystem::path &base_path, const std::string &book_id)
{
    return base_path / (book_id + ".cache");
}

/////////////////////////////////////
// Progress Store

std::filesystem::path progress_store_path_for_book(const std::filesystem::path &base_path, const std::string &book_id)
{
    return base_path / (book_id + ".progress");
}

void write_book_progress(const std::filesystem::path &path, uint32_t percent)
{
    string_unordered_map kv = {
        {PROGRESS_KEY, std::to_string(percent)}
    };

    write_key_value(path, kv);
}

std::optional<uint32_t> load_book_progress(const std::filesystem::path &path)
{
    auto kv = load_key_value(path);
    auto it = kv.find(PROGRESS_KEY);
    if (it == kv.end())
    {
        return std::nullopt;
    }
    return try_decode_uint(it->second);
}

/////////////////////////////////////
// Bookmarks Store

constexpr const char *BOOKMARK_COUNT_KEY = "count";

std::filesystem::path bookmarks_store_path_for_book(const std::filesystem::path &base_path, const std::string &book_id)
{
    return base_path / (book_id + ".bookmarks");
}

void write_book_bookmarks(const std::filesystem::path &path, const std::vector<Bookmark> &bookmarks)
{
    string_unordered_map kv = {
        {BOOKMARK_COUNT_KEY, std::to_string(bookmarks.size())}
    };

    for (size_t i = 0; i < bookmarks.size(); ++i)
    {
        kv[std::to_string(i) + "_address"] = encode_address(bookmarks[i].address);
        kv[std::to_string(i) + "_label"] = bookmarks[i].label;
    }

    write_key_value(path, kv);
}

std::vector<Bookmark> load_book_bookmarks(const std::filesystem::path &path)
{
    std::vector<Bookmark> bookmarks;

    auto kv = load_key_value(path);
    auto count_it = kv.find(BOOKMARK_COUNT_KEY);
    if (count_it == kv.end())
    {
        return bookmarks;
    }

    auto count = try_decode_uint(count_it->second);
    if (!count)
    {
        return bookmarks;
    }

    for (uint32_t i = 0; i < *count; ++i)
    {
        auto address_it = kv.find(std::to_string(i) + "_address");
        if (address_it == kv.end())
        {
            continue;
        }

        auto label_it = kv.find(std::to_string(i) + "_label");
        bookmarks.push_back(Bookmark{
            decode_address(address_it->second),
            label_it != kv.end() ? label_it->second : std::string()
        });
    }

    return bookmarks;
}

} // namespace

StateStore::StateStore(std::filesystem::path base_dir)
    : activity_store_path(base_dir / "activity"),
      book_data_root_path(base_dir / "books"),
      settings_store_path(base_dir / "settings"),
      settings(load_key_value(settings_store_path)),
      book_ids_store_path(base_dir / "book_ids"),
      book_ids(load_key_value(book_ids_store_path))
{
    std::filesystem::create_directories(base_dir);
    std::filesystem::create_directories(book_data_root_path);

    auto [browse_path, book_path] = load_activity_store(activity_store_path);
    current_browse_path = browse_path;
    current_book_path = book_path;
}

StateStore::~StateStore()
{
}

const std::optional<std::filesystem::path> &StateStore::get_current_browse_path() const
{
    return current_browse_path;
}

void StateStore::set_current_browse_path(std::filesystem::path path)
{
    if (current_browse_path != path)
    {
        current_browse_path = path;
        activity_dirty = true;
    }
}

void StateStore::remove_current_browse_path()
{
    if (current_browse_path)
    {
        current_browse_path.reset();
        activity_dirty = true;
    }
}

const std::optional<std::filesystem::path> &StateStore::get_current_book_path() const
{
    return current_book_path;
}

void StateStore::set_current_book_path(std::filesystem::path path)
{
    if (current_book_path != path)
    {
        current_book_path = path;
        activity_dirty = true;
    }
}

void StateStore::remove_current_book_path()
{
    if (current_book_path)
    {
        current_book_path.reset();
        activity_dirty = true;
    }
}

std::optional<DocAddr> StateStore::get_book_address(const std::string &book_id) const
{
    auto it = book_addresses.find(book_id);
    if (it != book_addresses.end())
    {
        return it->second;
    }

    auto cache = load_book_address(
        address_store_path_for_book(book_data_root_path, book_id)
    );
    if (cache)
    {
        book_addresses[book_id] = *cache;
    }

    return cache;
}

void StateStore::set_book_address(const std::string &book_id, DocAddr address)
{
    auto it = book_addresses.find(book_id);
    if (it == book_addresses.end() || it->second != address)
    {
        book_addresses[book_id] = address;
    }
}

const string_unordered_map &StateStore::get_reader_cache(const std::string &book_id) const
{
    auto it = book_reader_caches.find(book_id);
    if (it != book_reader_caches.end())
    {
        return it->second;
    }

    book_reader_caches[book_id] = load_key_value(
        reader_cache_store_path_for_book(book_data_root_path, book_id)
    );

    return book_reader_caches[book_id];
}

void StateStore::set_reader_cache(const std::string &book_id, const string_unordered_map &new_cache)
{
    const auto &cur_cache = get_reader_cache(book_id);

    if (cur_cache != new_cache)
    {
        book_reader_caches[book_id] = new_cache;
        reader_cache_dirty.emplace(book_id);
    }
}

std::optional<std::string> StateStore::get_book_id_for_path(const std::filesystem::path &path) const
{
    auto it = book_ids.find(path.string());
    if (it != book_ids.end())
    {
        return it->second;
    }
    return std::nullopt;
}

void StateStore::set_book_id_for_path(const std::filesystem::path &path, const std::string &book_id)
{
    auto key = path.string();
    auto it = book_ids.find(key);
    if (it == book_ids.end() || it->second != book_id)
    {
        book_ids[key] = book_id;
        book_ids_dirty = true;
    }
}

std::optional<uint32_t> StateStore::get_book_progress(const std::string &book_id) const
{
    auto it = book_progress.find(book_id);
    if (it != book_progress.end())
    {
        return it->second;
    }

    auto loaded = load_book_progress(
        progress_store_path_for_book(book_data_root_path, book_id)
    );
    if (loaded)
    {
        book_progress[book_id] = *loaded;
    }

    return loaded;
}

void StateStore::set_book_progress(const std::string &book_id, uint32_t percent)
{
    auto it = book_progress.find(book_id);
    if (it == book_progress.end() || it->second != percent)
    {
        book_progress[book_id] = percent;
        book_progress_dirty.emplace(book_id);
    }
}

const std::vector<Bookmark> &StateStore::get_bookmarks(const std::string &book_id) const
{
    auto it = book_bookmarks.find(book_id);
    if (it != book_bookmarks.end())
    {
        return it->second;
    }

    book_bookmarks[book_id] = load_book_bookmarks(
        bookmarks_store_path_for_book(book_data_root_path, book_id)
    );
    return book_bookmarks[book_id];
}

void StateStore::add_bookmark(const std::string &book_id, DocAddr address, const std::string &label)
{
    get_bookmarks(book_id); // ensure the cache is populated from disk first
    book_bookmarks[book_id].push_back(Bookmark{address, label});
    bookmarks_dirty.emplace(book_id);
}

void StateStore::remove_bookmark(const std::string &book_id, DocAddr address)
{
    get_bookmarks(book_id); // ensure the cache is populated from disk first
    auto &bookmarks = book_bookmarks[book_id];

    auto it = std::find_if(bookmarks.begin(), bookmarks.end(), [address](const Bookmark &b) {
        return b.address == address;
    });
    if (it != bookmarks.end())
    {
        bookmarks.erase(it);
        bookmarks_dirty.emplace(book_id);
    }
}

std::optional<std::string> StateStore::get_setting(const std::string &name) const
{
    auto it = settings.find(name);
    if (it != settings.end())
    {
        return it->second;
    }
    return std::nullopt;
}

void StateStore::set_setting(const std::string &name, const std::string &value)
{
    auto it = settings.find(name);
    if (it == settings.end() || it->second != value)
    {
        settings[name] = value;
        settings_dirty = true;
    }
}

void StateStore::flush() const
{
    if (activity_dirty)
    {
        write_activity_store(activity_store_path, *this);
        activity_dirty = false;
    }

    // book addresses
    {
        for (const auto &[book_id, address] : book_addresses)
        {
            write_book_address(
                address_store_path_for_book(book_data_root_path, book_id),
                address
            );
        }
        book_addresses.clear();
    }

    // book cache
    {
        for (const auto &book_id : reader_cache_dirty)
        {
            const auto &cache = book_reader_caches[book_id];
            write_key_value(
                reader_cache_store_path_for_book(book_data_root_path, book_id),
                cache
            );
        }
        reader_cache_dirty.clear();
    }

    if (settings_dirty)
    {
        write_key_value(settings_store_path, settings);
        settings_dirty = false;
    }

    if (book_ids_dirty)
    {
        write_key_value(book_ids_store_path, book_ids);
        book_ids_dirty = false;
    }

    // book progress
    {
        for (const auto &book_id : book_progress_dirty)
        {
            write_book_progress(
                progress_store_path_for_book(book_data_root_path, book_id),
                book_progress[book_id]
            );
        }
        book_progress_dirty.clear();
    }

    // bookmarks
    {
        for (const auto &book_id : bookmarks_dirty)
        {
            write_book_bookmarks(
                bookmarks_store_path_for_book(book_data_root_path, book_id),
                book_bookmarks[book_id]
            );
        }
        bookmarks_dirty.clear();
    }
}
