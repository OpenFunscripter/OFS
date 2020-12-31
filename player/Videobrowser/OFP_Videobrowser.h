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

    static constexpr int MaxThumbailProcesses = 4;
    static SDL_sem* ThumbnailThreadSem;

    std::string ClickedFilePath;
    std::string Filter;

    Videobrowser(VideobrowserSettings* settings);

	void ShowBrowser(const char* Id, bool* open) noexcept;

    inline void SetPath(const std::string& path) { if (path != settings->CurrentPath) { settings->CurrentPath = path; CacheNeedsUpdate = true; } }
    inline const std::string& Path() const { return settings->CurrentPath; }
};