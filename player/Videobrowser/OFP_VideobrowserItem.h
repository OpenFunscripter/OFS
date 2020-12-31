#pragma once

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
    void GenThumbail(bool startThread) noexcept;
    bool HasThumbnail = false;
    void IncrementRefCount() noexcept;
public:
    VideobrowserItem(const std::string& path, size_t byte_count, bool genThumb, bool matchingScript) noexcept;
    ~VideobrowserItem();

    VideobrowserItem(const VideobrowserItem& item) noexcept {
        *this = item;
        if (HasThumbnail) { IncrementRefCount(); }
    }

    VideobrowserItem(VideobrowserItem&& item) noexcept {
        *this = std::move(item);
        if (HasThumbnail) { IncrementRefCount(); }
    }

    VideobrowserItem& operator=(const VideobrowserItem&) noexcept = default;

    std::string filename;
    std::string path;
    std::string extension;

    inline bool IsDirectory() const {
        return extension.empty();
    }

    uint64_t GetTexId() const;
    uint64_t Id;
    bool HasMatchingScript = false;
    bool Focussed = false;
};
