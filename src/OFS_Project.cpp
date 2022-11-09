#include "OFS_Project.h"
#include "OFS_Localization.h"
#include "OFS_ImGui.h"

OFS_Project::OFS_Project() noexcept
{
    stateHandle = OFS_ProjectState<ProjectState>::Register(ProjectState::StateName);
    bookmarkStateHandle = OFS_ProjectState<ProjectBookmarkState>::Register(ProjectBookmarkState::StateName);
    Funscripts.emplace_back(std::move(std::make_shared<Funscript>()));
}

OFS_Project::~OFS_Project() noexcept
{

}

bool OFS_Project::Load(const std::string& path) noexcept
{
	valid = false;
	#if 1
	std::vector<uint8_t> projectBin;
	if(Util::ReadFile(path.c_str(), projectBin) > 0) {
		bool succ;
		auto projectState = Util::ParseCBOR(projectBin, &succ);
		if(succ) {
			OFS_StateManager::Get()->DeserializeProjectAll(projectState);
			valid = true;
		}
	}
	#else
	std::string projectJson = Util::ReadFileString(path.c_str());
	if(!projectJson.empty()) {
		bool succ;
		auto json = Util::ParseJson(projectJson, &succ);
		if(succ) {
			OFS_StateManager::Get()->DeserializeProjectAll(json);
			valid = true;
		}
	}
	#endif

	if(valid) {
		auto& projectState = State();
		OFS_Binary::Deserialize(projectState.binaryFunscriptData, *this);

		lastPath = path;
	}

	return valid;
}

bool OFS_Project::Import(const std::string& file) noexcept
{
	FUN_ASSERT(!valid, "Can't import if project is already loaded.");
	valid = false;

	auto& projectState = State();
	auto basePath = Util::PathFromString(file);

	lastPath = basePath.replace_extension(OFS_Project::Extension).u8string();

	// FIXME: assumes media
	basePath = Util::PathFromString(file);
	if(Util::FileExists(file)) {
		projectState.mediaPath = file;
		auto funscriptPath = basePath; 
		auto funscriptPathStr = funscriptPath.replace_extension(".funscript").u8string();

		auto script = std::make_shared<Funscript>();
		if(script->open(funscriptPathStr)) {
			Funscripts.clear();
			Funscripts.emplace_back(std::move(script));
			valid = true;
		}
	}

	return valid;
}

void OFS_Project::Save(const std::string& path, bool clearUnsavedChanges) noexcept
{	
	{
		auto& projectState = State();
		projectState.binaryFunscriptData.clear();
		auto size = OFS_Binary::Serialize(projectState.binaryFunscriptData, *this);
		projectState.binaryFunscriptData.resize(size);
	}
	auto projectState = OFS_StateManager::Get()->SerializeProjectAll();

	#if 1
	auto projectBin = Util::SerializeCBOR(projectState);
	Util::WriteFile(path.c_str(), projectBin.data(), projectBin.size());
	#else
	auto projectJson = Util::SerializeJson(projectState, true);
	Util::WriteFile(path.c_str(), projectJson.data(), projectJson.size());
	#endif
	if(clearUnsavedChanges) {
		for(auto& script : Funscripts) {
			script->SetSavedFromOutside();
		}
	}
}

void OFS_Project::Update(float delta, bool idleMode) noexcept
{
    if(!idleMode) {
        auto& projectState = State();
		projectState.activeTimer += delta;
	}
}

bool OFS_Project::HasUnsavedEdits() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    for(auto& script : Funscripts) {
        if(script->HasUnsavedEdits()) {
            return true;
        }
    }
    return false;
}

void OFS_Project::ShowProjectWindow(bool* open) noexcept
{
	if (*open) {
		ImGui::OpenPopup(TR_ID("PROJECT", Tr::PROJECT));
	}

	if (ImGui::BeginPopupModal(TR_ID("PROJECT", Tr::PROJECT), open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize))
	{
		OFS_PROFILE(__FUNCTION__);
        auto& projectState = State();
        auto& Metadata = projectState.metadata;
		ImGui::PushID(Metadata.title.c_str());
		
		ImGui::Text("%s: %s", TR(MEDIA), projectState.mediaPath.c_str()); 
		OFS::Tooltip(projectState.mediaPath.c_str());
		
		Util::FormatTime(Util::FormatBuffer, sizeof(Util::FormatBuffer), projectState.activeTimer, true);
		ImGui::Text("%s: %s", TR(TIME_SPENT), Util::FormatBuffer);
		ImGui::Separator();

		ImGui::Spacing();
		ImGui::TextDisabled(TR(SCRIPTS));
		for (auto& script : Funscripts) {
			if (ImGui::Button(script->Title.c_str(), ImVec2(-1.f, 0.f))) {
				Util::SaveFileDialog(TR(CHANGE_DEFAULT_LOCATION),
					script->Path(),
					[&](auto result) {
						if (!result.files.empty()) {
							auto newPath = Util::PathFromString(result.files[0]);
							if (newPath.extension().u8string() == ".funscript") {
								script->UpdatePath(newPath.u8string());
							}
						}
					});
			}
			OFS::Tooltip(TR(CHANGE_LOCATION));
		}
		ImGui::PopID();
		ImGui::EndPopup();
	}
}