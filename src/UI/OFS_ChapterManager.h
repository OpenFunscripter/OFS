#pragma once

#include <cstdint>

class OFS_ChapterManager
{
    private:
    uint32_t stateHandle = 0xFFFF'FFFF;

    public:
    OFS_ChapterManager() noexcept;
    OFS_ChapterManager(const OFS_ChapterManager&) = delete;
    OFS_ChapterManager(OFS_ChapterManager&&) = delete;
    ~OFS_ChapterManager() noexcept;
};