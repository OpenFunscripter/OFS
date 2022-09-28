#include "OFS_ScriptSettings.h"

#include "OFS_Util.h"

OFS_VideoplayerWindow::OFS_VideoPlayerSettings* OFS_ScriptSettings::player = nullptr;

void OFS_ScriptSettings::AddBookmark(Bookmark&& bookmark) noexcept
{
	// when can create a bookmark "a" followed by "a_end" 
	// this will set "a" as a start marker
	if (!Bookmarks.empty()) {
		auto it = std::find_if(Bookmarks.begin(), Bookmarks.end(),
			[&](auto& mark) {
				return mark.atS > bookmark.atS;
			});
		if (it != Bookmarks.begin()) it--;

		if (bookmark.type == Bookmark::BookmarkType::END_MARKER) {
			if (it->type == Bookmark::BookmarkType::REGULAR) {
				auto name_copy = bookmark.name;
				name_copy.erase(name_copy.end() - sizeof(Bookmark::endMarker) + 1, name_copy.end());
				if (Util::StringEqualsInsensitive(name_copy, it->name)) {
					it->type = Bookmark::BookmarkType::START_MARKER;
				}
			}
		}
		else {
			if (it + 1 != Bookmarks.end() && it->type != Bookmark::BookmarkType::END_MARKER) it++;
			if (it->type == Bookmark::BookmarkType::END_MARKER) {
				auto name_copy = it->name;
				name_copy.erase(name_copy.end() - sizeof(Bookmark::endMarker) + 1, name_copy.end());
				if (Util::StringEqualsInsensitive(name_copy, bookmark.name)) {
					bookmark.type = Bookmark::BookmarkType::START_MARKER;
				}
			}
		}
	}

	Bookmarks.emplace_back(bookmark);
	std::sort(Bookmarks.begin(), Bookmarks.end(),
		[](auto& a, auto& b) { return a.atS < b.atS; }
	);
}
