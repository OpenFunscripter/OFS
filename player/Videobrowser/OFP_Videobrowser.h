#pragma once

#include <vector>
#include <string>
#include <array>
#include <tuple>
#include <filesystem>

#include "OFP_VideobrowserItem.h"

#include "OFS_Reflection.h"
#include "SDL_atomic.h"
#include "SDL_thread.h"

class VideobrowserEvents {
public:
    static int32_t VideobrowserItemClicked;
    static void RegisterEvents() noexcept;
};

struct VideobrowserSettings {

	std::string CurrentPath = "/";
    int32_t ItemsPerRow = 5;

    bool showThumbnails = true;

    struct LibraryPath {
        std::string path;
        bool recursive = false;
        template <class Archive>
        inline void reflect(Archive& ar) {
            OFS_REFLECT(path, ar);
            OFS_REFLECT(recursive, ar);
        }
    };
    std::vector<LibraryPath> SearchPaths;

    template <class Archive>
    inline void reflect(Archive& ar) {
        OFS_REFLECT(showThumbnails, ar);
        OFS_REFLECT(SearchPaths, ar);
        OFS_REFLECT(CurrentPath, ar);
        OFS_REFLECT(ItemsPerRow, ar);
    }
};

struct LibraryCachedVideos {
    struct CachedVideo {
        std::string path;
        uint64_t byte_count = 0;
        uint64_t timestamp = 0;

        bool thumbnail = false;
        bool hasScript = false;

        template <class Archive>
        inline void reflect(Archive& ar) {
            OFS_REFLECT(path, ar);
            OFS_REFLECT(byte_count, ar);
            OFS_REFLECT(timestamp, ar);
            OFS_REFLECT(thumbnail, ar);
            OFS_REFLECT(hasScript, ar);
        }
    };
    std::vector<CachedVideo> videos;

    template <class Archive>
    inline void reflect(Archive& ar) {
        OFS_REFLECT(videos, ar);
    }


    std::string cachePath;
    void load(const std::string& path) noexcept;
    void save() noexcept;
};


class Videobrowser {
private:
    void updateCache(const std::string& path) noexcept;

    void updateLibraryCache(bool useCache) noexcept;

#ifdef WIN32
    void chooseDrive() noexcept;
#endif
    bool CacheNeedsUpdate = true;

    bool UpdateLibCache = false;

    SDL_SpinLock ItemsLock = 0;
    std::vector<VideobrowserItem> Items;
    LibraryCachedVideos libCache;

    void renderLoot() noexcept;
public:
    VideobrowserSettings* settings = nullptr;

	static constexpr const char* VideobrowserId = "Videobrowser";
    static constexpr const char* VideobrowserSceneId = "VideobrowserScene";
    static constexpr const char* VideobrowserSettingsId = "Videobrowser Settings";
    static constexpr const char* VideobrowserRandomId = "Random video";

    static constexpr int MaxThumbailProcesses = 4;
    static SDL_sem* ThumbnailThreadSem;

    std::string ClickedFilePath;
    std::string Filter;

    bool ShowSettings = false;
    bool Random = true;

    Videobrowser(VideobrowserSettings* settings);
    ~Videobrowser();

	void ShowBrowser(const char* Id, bool* open) noexcept;
    void ShowBrowserSettings(bool* open) noexcept;

    void Lootcrate(bool* open) noexcept;

    inline void SetPath(const std::string& path) { if (path != settings->CurrentPath) { settings->CurrentPath = path; CacheNeedsUpdate = true; } }
    inline const std::string& Path() const { return settings->CurrentPath; }
};