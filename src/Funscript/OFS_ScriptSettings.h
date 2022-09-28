#pragma once

#include <cstdint>

#include <string>
#include <vector>

#include "OFS_Reflection.h"
#include "OFS_Util.h"
#include "OFS_VideoplayerWindow.h"


struct OFS_ScriptSettings {
	struct Bookmark {
		enum class BookmarkType : uint8_t {
			REGULAR,
			START_MARKER,
			END_MARKER
		};
		float atS; // floating point seconds
		std::string name;
		BookmarkType type = BookmarkType::REGULAR;

		static constexpr char startMarker[] = "_start";
		static constexpr char endMarker[] = "_end";
		
		Bookmark() noexcept {}
		Bookmark(std::string&& name, float atSeconds) noexcept
			: name(std::move(name)), atS(atSeconds)
		{
			UpdateType();
		}
		Bookmark(const std::string& name, float atSeconds) noexcept
			: name(name), atS(atSeconds)
		{
			UpdateType();
		}

		inline void UpdateType() noexcept {

			Util::trim(name);

			if (Util::StringEqualsInsensitive(name, startMarker) || Util::StringEqualsInsensitive(name, endMarker)) {
				type = BookmarkType::REGULAR;
				return;
			}

			if (Util::StringEndsWith(name, startMarker)) {
				type = BookmarkType::START_MARKER;
				name.erase(name.end() - sizeof(startMarker) + 1, name.end());
			}
			else if (Util::StringEndsWith(name, endMarker)) {
				type = BookmarkType::END_MARKER;
			}
			else {
				type = BookmarkType::REGULAR;
			}
		}

		template<typename S>
		void serialize(S& s)
		{
			s.ext(*this, bitsery::ext::Growable{},
				[](S& s, Bookmark& o) {
					s.text1b(o.name, o.name.max_size());
					s.value4b(o.atS);
					s.value1b(o.type);
				});
		}
	};

	std::string version = "OFS " OFS_LATEST_GIT_TAG "@" OFS_LATEST_GIT_HASH;
	std::vector<Bookmark> Bookmarks;
	float lastPlayerPosition = 0.f;
	static OFS_VideoplayerWindow::OFS_VideoPlayerSettings* player;

	struct TempoModeSettings {
		float bpm = 100.f;
		float beatOffsetSeconds = 0.f;
		int measureIndex = 0;
		
		template<typename S>
		void serialize(S& s)
		{
			s.ext(*this, bitsery::ext::Growable{},
				[](S& s, TempoModeSettings& o) {
					s.value4b(o.bpm);
					if(o.bpm <= 0.f || std::isnan(o.bpm)) {
						o.bpm = 100.f;
					}
					s.value4b(o.beatOffsetSeconds);
					s.value4b(o.measureIndex);
				});
		}

	} tempoSettings;

	template<typename S>
	void serialize(S& s)
	{
		s.ext(*this, bitsery::ext::Growable{},
			[](S& s, OFS_ScriptSettings& o) {
				s.text1b(o.version, o.version.max_size());
				s.container(o.Bookmarks, o.Bookmarks.max_size());
				s.value4b(o.lastPlayerPosition);
				s.object(o.tempoSettings);
				FUN_ASSERT(o.player != nullptr, "player not set");
				s.object(*o.player);
			});
	}

	// bookmarks
	void AddBookmark(Bookmark&& bookmark) noexcept;
};