#pragma once

#include <vector>
#include <string>
#include <array>
#include <tuple>
#include <filesystem>

#include "OFS_Reflection.h"

#include "SDL_atomic.h"

static std::array<std::pair<const char*, bool>, 10> BrowserExtensions {
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


class VideobrowserEvents {
public:
    static int32_t VideobrowserItemClicked;
    static void RegisterEvents() noexcept;
};

struct VideobrowserSettings {

	std::string CurrentPath = "/";
    int32_t ItemsPerRow = 5;

    template <class Archive>
    inline void reflect(Archive& ar) {
        OFS_REFLECT(CurrentPath, ar);
        OFS_REFLECT(ItemsPerRow, ar);
    }
};

class Videobrowser {
private:
    void updateCache(const std::string& path) noexcept;
#ifdef WIN32
    void chooseDrive() noexcept;
#endif
    bool CacheNeedsUpdate = true;

    SDL_SpinLock ItemsLock = 0;
    std::vector<VideobrowserItem> Items;
    VideobrowserSettings* settings = nullptr;
public:
	static constexpr const char* VideobrowserId = "Videobrowser";
    static constexpr const char* VideobrowserSceneId = "VideobrowserScene";

    std::string ClickedFilePath;

    Videobrowser(VideobrowserSettings* settings);

	void ShowBrowser(const char* Id, bool* open) noexcept;

    inline void SetPath(const std::string& path) { if (path != settings->CurrentPath) { settings->CurrentPath = path; CacheNeedsUpdate = true; } }
    inline const std::string& Path() const { return settings->CurrentPath; }
};