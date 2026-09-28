#include "./epub_cover.h"

#include "./epub_metadata.h"
#include "util/zip_utils.h"

#include <zip.h>

std::optional<EpubCoverImage> get_epub_cover_image(const std::filesystem::path &epub_path)
{
    int err = 0;
    zip_t *zip = zip_open(epub_path.c_str(), ZIP_RDONLY, &err);
    if (zip == nullptr)
    {
        return std::nullopt;
    }

    std::optional<EpubCoverImage> result;

    auto container_xml = read_zip_file_str(zip, EPUB_CONTAINER_PATH);
    if (!container_xml.empty())
    {
        auto rootfile_path = epub_parse_rootfile_path(container_xml.data());
        if (!rootfile_path.empty())
        {
            auto package_xml = read_zip_file_str(zip, rootfile_path);
            if (!package_xml.empty())
            {
                PackageContents package;
                if (
                    epub_parse_package_contents(rootfile_path, package_xml.data(), package) &&
                    !package.cover_manifest_id.empty()
                )
                {
                    auto item = package.id_to_manifest_item.find(package.cover_manifest_id);
                    if (item != package.id_to_manifest_item.end())
                    {
                        auto image_data = read_zip_file_str(zip, item->second.href_absolute);
                        auto extension = std::filesystem::path(item->second.href).extension().string();
                        if (!image_data.empty() && extension.size() > 1)
                        {
                            result = EpubCoverImage{
                                std::move(image_data),
                                extension.substr(1) // drop leading '.'
                            };
                        }
                    }
                }
            }
        }
    }

    zip_close(zip);

    return result;
}
