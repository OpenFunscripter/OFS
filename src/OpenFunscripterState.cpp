#include "OpenFunscripterState.h"

OFS_REGISTER_STATE(OpenFunscripterState);

void OpenFunscripterState::addRecentFile(const RecentFile& recentFile) noexcept
{
    auto it = std::find_if(recentFiles.begin(), recentFiles.end(),
		[&](auto& file) {
			return file.projectPath == recentFile.projectPath;
		});
	if (it != recentFiles.end()) {
		recentFiles.erase(it);
	}
	recentFiles.push_back(recentFile);
	if (recentFiles.size() > 5) {
		recentFiles.erase(recentFiles.begin());
	}
}