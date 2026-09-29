#include "./battery.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace
{

std::optional<int> read_plain_integer_file(const char *path)
{
    std::ifstream file(path);
    if (!file)
    {
        return std::nullopt;
    }

    int percent = -1;
    file >> percent;
    if (!file || percent < 0 || percent > 100)
    {
        return std::nullopt;
    }

    return percent;
}

// Onion OS keeps a small JSON status file at /tmp/.axp_result, refreshed by
// an existing background process (no PMIC/I2C access needed on our part),
// e.g. {"battery":76,"voltage":4051,"charging":false}. Parsed by hand
// (rather than pulling in a JSON library) since we only need one integer
// field out of a known-simple, flat object.
std::optional<int> read_axp_result_file(const char *path)
{
    std::ifstream file(path);
    if (!file)
    {
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string contents = buffer.str();

    size_t key_pos = contents.find("\"battery\"");
    if (key_pos == std::string::npos)
    {
        return std::nullopt;
    }

    size_t colon_pos = contents.find(':', key_pos);
    if (colon_pos == std::string::npos)
    {
        return std::nullopt;
    }

    size_t digits_start = colon_pos + 1;
    while (digits_start < contents.size() && std::isspace(static_cast<unsigned char>(contents[digits_start])))
    {
        ++digits_start;
    }

    size_t digits_end = digits_start;
    while (digits_end < contents.size() && std::isdigit(static_cast<unsigned char>(contents[digits_end])))
    {
        ++digits_end;
    }

    // A valid percentage is at most 3 digits (0-100) - bail out rather than
    // risk std::stoi overflowing on anything unexpected.
    if (digits_end == digits_start || digits_end - digits_start > 3)
    {
        return std::nullopt;
    }

    int percent = std::stoi(contents.substr(digits_start, digits_end - digits_start));
    if (percent < 0 || percent > 100)
    {
        return std::nullopt;
    }

    return percent;
}

} // namespace

std::optional<int> get_battery_percent()
{
#if PLATFORM_MIYOO_MINI
    // Try known paths in order - firmware varies (OnionOS vs MiniUI vs
    // stock), so we cannot assume only one of these exists. `sandbox
    // battery` prints which one (if any) worked.
    if (auto percent = read_axp_result_file("/tmp/.axp_result"))
    {
        return percent;
    }
    if (auto percent = read_plain_integer_file("/tmp/battery"))
    {
        return percent;
    }
    return std::nullopt;
#else
    return std::nullopt;
#endif
}
