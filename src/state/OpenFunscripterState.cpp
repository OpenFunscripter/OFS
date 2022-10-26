#include "OpenFunscripterState.h"
#include "ScriptModeState.h"
#include "Simulator3dState.h"
#include "MetadataEditorState.h"
#include "PreferenceState.h"

void OpenFunscripterState::RegisterAll() noexcept
{
	OFS_REGISTER_STATE(OpenFunscripterState);
	OFS_REGISTER_STATE(ScriptingModeState);
	OFS_REGISTER_STATE(Simulator3dState);
	OFS_REGISTER_STATE(FunscriptMetadataState);
	OFS_REGISTER_STATE(PreferenceState);
}

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