#pragma once

#include <string>

#include "OFS_Reflection.h"
#include "OFP_Videobrowser.h"
#include "OFS_Videoplayer.h"

enum OFP_Scene : int32_t {
	Player,
	Filebrowser,
	TotalScenes
};


struct OFP_Settings {
	std::string font_override = "";
	float default_font_size = 18.f;
	OFP_Scene ActiveScene = OFP_Scene::Player;

	std::string last_file;

	bool vsync = true;
	bool show_video = true;

	bool show_browser = false;
	bool show_tcode = false;
	bool show_timeline = true;
	bool show_controls = true;
	bool show_time = true;

	VideobrowserSettings videoBrowser;
	VideoplayerWindow::OFS_VideoPlayerSettings* videoPlayer = nullptr;

	template <class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(videoBrowser, ar);
		OFS_REFLECT(ActiveScene, ar);
		OFS_REFLECT(last_file, ar);

		OFS_REFLECT_PTR(videoPlayer, ar);

		OFS_REFLECT(show_timeline, ar);
		OFS_REFLECT(show_controls, ar);
		OFS_REFLECT(show_time, ar);
		OFS_REFLECT(show_browser, ar);
		OFS_REFLECT(show_tcode, ar);

		OFS_REFLECT(font_override, ar);
		OFS_REFLECT(default_font_size, ar);
		OFS_REFLECT(vsync, ar);
		OFS_REFLECT(show_video, ar);
	}

	std::string loadedPath;
	bool load(const std::string& path) noexcept;
	void save() noexcept;
};