#pragma once

#include <vector>
#include <string>
#include <array>
#include <tuple>
#include <filesystem>

#include "OFP_Videopreview.h"
#include "OFP_VideobrowserItem.h"

#include "OFS_Reflection.h"
#include "SDL_atomic.h"
#include "SDL_thread.h"

#include "OFP_Sqlite.h"

class VideobrowserEvents {
public:
    static int32_t VideobrowserItemClicked;
    static void RegisterEvents() noexcept;
};

struct VideobrowserSettings {

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
        OFS_REFLECT(ItemsPerRow, ar);
    }
};


class Videobrowser {
private:
    void updateLibraryCache() noexcept;


    bool CacheNeedsUpdate = false;

    SDL_SpinLock ItemsLock = 0;
    std::vector<VideobrowserItem> Items;

    void renderLoot() noexcept;
public:
    VideobrowserSettings* settings = nullptr;

    uint64_t previewItemId = 0;
    VideoPreview preview;

	static constexpr const char* VideobrowserId = "Videobrowser";
    static constexpr const char* VideobrowserSceneId = "VideobrowserScene";
    static constexpr const char* VideobrowserSettingsId = "Videobrowser Settings";
    static constexpr const char* VideobrowserRandomId = "Random video";

    static constexpr int MaxThumbailProcesses = 4;
    static SDL_sem* ThumbnailThreadSem;

    std::string ClickedFilePath;
    std::string Filter;

    bool ShowSettings = false;
    bool Random = false;

    Videobrowser(VideobrowserSettings* settings);
    ~Videobrowser();

	void ShowBrowser(const char* Id, bool* open) noexcept;
    void ShowBrowserSettings(bool* open) noexcept;

    void Lootcrate(bool* open) noexcept;
};