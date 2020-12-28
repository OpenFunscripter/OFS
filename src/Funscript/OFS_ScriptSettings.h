#pragma once

#include <cstdint>

#include <string>
#include <vector>

#include "OFS_Reflection.h"
#include "OFS_Util.h"
#include "OFS_Videoplayer.h"


struct OFS_ScriptSettings {
	struct Bookmark {
		enum BookmarkType {
			REGULAR,
			START_MARKER,
			END_MARKER
		};
		int32_t at;
		std::string name;
		BookmarkType type = BookmarkType::REGULAR;

		static constexpr char startMarker[] = "_start";
		static constexpr char endMarker[] = "_end";
		Bookmark() {}

		Bookmark(const std::string& name, int32_t at)
			: name(name), at(at)
		{
			UpdateType();
		}

		inline void UpdateType() noexcept {

			Util::trim(name);

			if (Util::StringEqualsInsensitive(name, startMarker) || Util::StringEqualsInsensitive(name, endMarker)) {
				type = BookmarkType::REGULAR;
				return;
			}

			if (Util::StringEndswith(name, startMarker)) {
				type = BookmarkType::START_MARKER;
				name.erase(name.end() - sizeof(startMarker) + 1, name.end());
			}
			else if (Util::StringEndswith(name, endMarker)) {
				type = BookmarkType::END_MARKER;
				// don't remove _end because it helps distinguish the to markers
				//name.erase(name.end() - sizeof(endMarker) + 1, name.end());
			}
			else {
				type = BookmarkType::REGULAR;
			}
		}

		template <class Archive>
		inline void reflect(Archive& ar) {
			OFS_REFLECT(at, ar);
			OFS_REFLECT(name, ar);
			OFS_REFLECT(type, ar);

			// HACK: convert existing bookmarks to their correct type
			if (type != BookmarkType::START_MARKER) {
				UpdateType();
			}
		}
	};

	std::string version = "OFS " OFS_LATEST_GIT_TAG "@" OFS_LATEST_GIT_HASH;
	std::vector<Bookmark> Bookmarks;
	int32_t last_pos_ms = 0;
	std::vector<std::string> associatedScripts;
	static VideoplayerWindow::OFS_VideoPlayerSettings* player;

	struct TempoModeSettings {
		int bpm = 100;
		float beat_offset_seconds = 0.f;
		int multiIDX = 0;

		template <class Archive>
		inline void reflect(Archive& ar) {
			OFS_REFLECT(bpm, ar);
			OFS_REFLECT(beat_offset_seconds, ar);
			OFS_REFLECT(multiIDX, ar);
		}

	} tempoSettings;


	template <class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(version, ar);
		OFS_REFLECT(tempoSettings, ar);
		OFS_REFLECT(associatedScripts, ar);
		OFS_REFLECT(Bookmarks, ar);
		OFS_REFLECT(last_pos_ms, ar);
		OFS_REFLECT_PTR(player, ar);
	}

	// bookmarks
	void AddBookmark(Bookmark&& bookmark) noexcept;
};