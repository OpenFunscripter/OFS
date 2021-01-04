#pragma once

#include "OFS_Texture.h"
#include "OFS_Reflection.h"

#include <string>
#include <tuple>
#include <array>

#include "OFP_Sqlite.h"

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
    Video video;
    OFS_Texture::Handle texture;
    uint64_t ThumbnailHash = 0;
    bool Focussed = false;

    VideobrowserItem(Video&& vid) noexcept;
    void GenThumbail() noexcept;
};
