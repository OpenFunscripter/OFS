#include "ProjectBookmarkState.h"
#include "OFS_Util.h"

void Bookmark::UpdateType() noexcept
{
    Util::trim(name);
    if (Util::StringEqualsInsensitive(name, startMarker) || Util::StringEqualsInsensitive(name, endMarker)) {
        type = BookmarkType::Regular;
        return;
    }

    if (Util::StringEndsWith(name, startMarker)) {
        type = BookmarkType::StartMarker;
        name.erase(name.end() - sizeof(startMarker) + 1, name.end());
    }
    else if (Util::StringEndsWith(name, endMarker)) {
        type = BookmarkType::EndMarker;
    }
    else {
        type = BookmarkType::Regular;
    }
}

void ProjectBookmarkState::AddBookmark(Bookmark&& bookmark) noexcept
{
	if (!Bookmarks.empty()) {
		auto it = std::find_if(Bookmarks.begin(), Bookmarks.end(),
			[&](auto& mark) {
				return mark.atS > bookmark.atS;
			});
		if (it != Bookmarks.begin()) it--;

		if (bookmark.type == BookmarkType::EndMarker) {
			if (it->type == BookmarkType::Regular) {
				auto name_copy = bookmark.name;
				name_copy.erase(name_copy.end() - sizeof(Bookmark::endMarker) + 1, name_copy.end());
				if (Util::StringEqualsInsensitive(name_copy, it->name)) {
					it->type = BookmarkType::StartMarker;
				}
			}
		}
		else {
			if (it + 1 != Bookmarks.end() && it->type != BookmarkType::EndMarker) it++;
			if (it->type == BookmarkType::EndMarker) {
				auto name_copy = it->name;
				name_copy.erase(name_copy.end() - sizeof(Bookmark::endMarker) + 1, name_copy.end());
				if (Util::StringEqualsInsensitive(name_copy, bookmark.name)) {
					bookmark.type = BookmarkType::StartMarker;
				}
			}
		}
	}

	Bookmarks.emplace_back(bookmark);
	std::sort(Bookmarks.begin(), Bookmarks.end(),
		[](auto& a, auto& b) { return a.atS < b.atS; }
	);
}