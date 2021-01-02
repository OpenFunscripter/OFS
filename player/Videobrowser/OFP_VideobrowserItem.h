#pragma once

#include "OFS_Texture.h"

#include "OFS_Reflection.h"

#include <string>
#include <tuple>
#include <array>

static std::array<std::pair<const char*, bool>, 10> BrowserExtensions{
    std::make_pair(".mp4", true),
    std::make_pair(".mkv", true),
    std::make_pair(".webm",true),
    std::make_pair(".wmv", true),
    std::make_pair(".avi", true),
    std::make_pair(".m4v", true),

    std::make_pair(".mp3", false),
    std::make_pair(".flac",false),
    std::make_pair(".wmv", false),
    std::make_pair(".ogg", false),
};

class VideobrowserItem {
private:
    bool GenThumbnailStarted = false;
public:
    VideobrowserItem(const std::string& path, size_t byte_count, uint64_t lastEdit, bool genThumb, bool matchingScript) noexcept;

    std::string filename;
    std::string path;
    std::string extension;

    inline bool IsDirectory() const {
        return extension.empty();
    }
    void GenThumbail() noexcept;

    OFS_Texture::Handle texture;

    uint64_t Id = 0;
    uint64_t lastEdit = 0;
    bool HasThumbnail = false;
    bool HasMatchingScript = false;
    bool Focussed = false;
};
