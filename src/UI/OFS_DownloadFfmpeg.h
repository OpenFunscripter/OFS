#pragma once
#include "imgui.h"

struct OFS_DownloadFfmpeg
{
    static constexpr const char* WindowId = "###DOWNLOAD_FFMPEG";
    static ImGuiID ModalId;
    static bool FfmpegMissing;
    static void DownloadFfmpegModal() noexcept;
};