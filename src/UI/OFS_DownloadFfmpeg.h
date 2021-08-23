#pragma once
#include "imgui.h"

struct OFS_DownloadFfmpeg
{
    static constexpr const char* ModalText = "Download ffmpeg?";
    static ImGuiID ModalId;
    static bool FfmpegMissing;
    static void DownloadFfmpegModal() noexcept;
};