#ifndef EPUB_COVER_H_
#define EPUB_COVER_H_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct EpubCoverImage
{
    std::vector<char> data;
    std::string extension; // e.g. "jpg", "png" - needed to decode `data`
};

// Returns the epub's cover image, if one can be found via the EPUB3
// manifest properties="cover-image" marker or the EPUB2
// <meta name="cover" content="..."> convention. Doesn't construct a full
// DocReader (no spine/doc-index/toc parsing) - just enough to locate and
// read one image, cheap enough to call for every visible gallery tile.
std::optional<EpubCoverImage> get_epub_cover_image(const std::filesystem::path &epub_path);

#endif
