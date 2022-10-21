#include "OFS_Preferences.h"
#include "OFS_Util.h"
#include "OpenFunscripter.h"
#include "OFS_Localization.h"

#include "OFS_Serialization.h"
#include "OFS_ImGui.h"

#include "imgui.h"
#include "imgui_stdlib.h"

OFS_Settings::OFS_Settings(const std::string& config) noexcept
	: config_path(config)
{
	bool success = false;
	if (Util::FileExists(config)) {
		auto jsonText = Util::ReadFileString(config.c_str());
		configObj = Util::ParseJson(jsonText, &success);
		if (!success) {
			LOGF_ERROR("Failed to parse config @ \"%s\"", config.c_str());
			configObj = nlohmann::json();
		}
	}

	if (!configObj[ConfigStr].is_object()) {
		configObj[ConfigStr] = nlohmann::json::object();
	}
	else {
		loadConfig();
	}
}

void OFS_Settings::saveConfig() noexcept
{
	auto jsonText = Util::SerializeJson(configObj, true);
	Util::WriteFile(config_path.c_str(), jsonText.data(), jsonText.size());
}

void OFS_Settings::loadConfig() noexcept
{
	OFS::Serializer::Deserialize(scripterSettings, config());
}

void OFS_Settings::saveSettings()
{
	OFS::Serializer::Serialize(scripterSettings, config());
	saveConfig();
}

static void copyTranslationHelper() noexcept
{
	auto srcDir = Util::Basepath() / "data" / OFS_Translator::TranslationDir;
	auto targetDir = Util::Prefpath(OFS_Translator::TranslationDir);
	std::error_code ec;
	std::filesystem::directory_iterator langDirIt(srcDir, ec);
	for(auto& pIt : langDirIt) {
		if(pIt.path().extension() == ".csv") {
			auto targetFile = targetDir / pIt.path().filename();
			if(Util::FileExists(targetFile.u8string())) {
				// merge the two
				auto input1 = pIt.path().u8string();
				auto input2 = targetFile.u8string();
				if(OFS_Translator::MergeIntoOne(input1.c_str(), input2.c_str(), input2.c_str())) {
					std::filesystem::remove(pIt.path(), ec);
				}
			}
			else {
				std::filesystem::copy_file(pIt.path(), targetFile, ec);
				if(!ec) {
					std::filesystem::remove(pIt.path(), ec);
				}
			}
		}
	}
}

bool OFS_Settings::ShowPreferenceWindow() noexcept
{
	bool save = false;
	if (ShowWindow)
		ImGui::OpenPopup(TR_ID("PREFERENCES", Tr::PREFERENCES));
	if (ImGui::BeginPopupModal(TR_ID("PREFERENCES", Tr::PREFERENCES), &ShowWindow, ImGuiWindowFlags_AlwaysAutoResize))
	{
		OFS_PROFILE(__FUNCTION__);
		if (ImGui::BeginChild("prefTabChild", ImVec2(600.f, 360.f))) {
			if (ImGui::BeginTabBar("##PreferenceTabs"))
			{
				if (ImGui::BeginTabItem(TR(APPLICATION)))
				{
					if (ImGui::RadioButton(TR(DARK_MODE), (int*)&scripterSettings.currentTheme,
						static_cast<int32_t>(OFS_Theme::Dark))) {
						SetTheme((OFS_Theme)scripterSettings.currentTheme);
						save = true;
					}
					ImGui::SameLine();
					if (ImGui::RadioButton(TR(LIGHT_MODE), (int*)&scripterSettings.currentTheme,
						static_cast<int32_t>(OFS_Theme::Light))) {
						SetTheme((OFS_Theme)scripterSettings.currentTheme);
						save = true;
					}
					
					ImGui::Separator();

					ImGui::TextWrapped(TR(PREFERENCES_TXT));
					if (ImGui::InputInt(TR(FRAME_LIMIT), &scripterSettings.framerateLimit, 1, 10)) {
						scripterSettings.framerateLimit = Util::Clamp(scripterSettings.framerateLimit, 60, 300);
						save = true;
					}
					OFS::Tooltip(TR(FRAME_LIMIT_TOOLTIP));
					ImGui::SameLine();
					if (ImGui::Checkbox(TR(VSYNC), (bool*)&scripterSettings.vsync)) {
						scripterSettings.vsync = Util::Clamp(scripterSettings.vsync, 0, 1); // just in case...
						SDL_GL_SetSwapInterval(scripterSettings.vsync);
						save = true;
					}
					OFS::Tooltip(TR(VSYNC_TOOLTIP));
					ImGui::Separator();
					ImGui::InputText(TR(FONT), scripterSettings.fontOverride.empty() ? (char*)TR(DEFAULT_FONT) : (char*)scripterSettings.fontOverride.c_str(),
						scripterSettings.fontOverride.size(), ImGuiInputTextFlags_ReadOnly);
					ImGui::SameLine();
					if (ImGui::Button(TR(CHANGE))) {
						Util::OpenFileDialog(TR(CHOOSE_FONT), "",
							[&](auto& result) {
								if (result.files.size() > 0) {
									scripterSettings.fontOverride = result.files.back();
									OpenFunscripter::ptr->LoadOverrideFont(scripterSettings.fontOverride);
									save = true;
								}
							}, false, { "*.ttf", "*.otf" }, "Fonts (*.ttf, *.otf)");
					}
					ImGui::SameLine();
					if (ImGui::Button(TR(CLEAR))) {
						scripterSettings.fontOverride = "";
						EventSystem::SingleShot([](void* ctx) {
							// fonts can't be updated during a frame
							// this updates the font during event processing
							// which is not during the frame
							auto app = OpenFunscripter::ptr;
							app->LoadOverrideFont(app->settings->data().fontOverride);
							}, nullptr);
					}

					if (ImGui::InputInt(TR(FONT_SIZE), (int*)&scripterSettings.defaultFontSize, 1, 1)) {
						scripterSettings.defaultFontSize = Util::Clamp(scripterSettings.defaultFontSize, 8, 64);
						EventSystem::SingleShot([](void* ctx) {
							// fonts can't be updated during a frame
							// this updates the font during event processing
							// which is not during the frame
							auto app = OpenFunscripter::ptr;
							app->LoadOverrideFont(app->settings->data().fontOverride);
							}, nullptr);
						save = true;
					}
					if(ImGui::BeginCombo(TR_ID("LANGUAGE", Tr::LANGUAGE), data().languageCsv.empty() ? "English" : data().languageCsv.c_str()))
					{
						for(auto& file : translationFiles) {
							if(ImGui::Selectable(file.c_str(), file == data().languageCsv)) {
								if(OFS_Translator::ptr->LoadTranslation(file.c_str()))
								{
									data().languageCsv = file;
									OFS_DynFontAtlas::AddTranslationText();
								}
							}
						}
						ImGui::EndCombo();
					}
					if(ImGui::IsItemClicked(ImGuiMouseButton_Left))	{
						copyTranslationHelper();
						translationFiles.clear();
						std::error_code ec;
						std::filesystem::directory_iterator dirIt(Util::Prefpath(OFS_Translator::TranslationDir), ec);
						for (auto& pIt : dirIt) {
							if(pIt.path().extension() == ".csv") {
								translationFiles.emplace_back(pIt.path().filename().u8string());
							}
						}
					}
					ImGui::SameLine();
					if(ImGui::Button(TR(RESET))) {
						data().languageCsv = std::string();
						OFS_Translator::ptr->LoadDefaults();
					}
					ImGui::SameLine();
					if(ImGui::Button(FMT("%s###DIRECTORY_TRANSLATION", ICON_FOLDER_OPEN)))
					{
						Util::OpenFileExplorer(Util::Prefpath(OFS_Translator::TranslationDir));
					}
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(TR(VIDEOPLAYER))) {
					if (ImGui::Checkbox(TR(FORCE_HW_DECODING), &scripterSettings.forceHwDecoding)) {
						save = true;
					}
					OFS::Tooltip(TR(FORCE_HW_DECODING_TOOLTIP));
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(TR(SCRIPTING)))
				{
					if(ImGui::Checkbox(TR_ID("HighlightEnable", Tr::ENABLE_MAX_SPEED_HIGHLIGHT), &BaseOverlay::ShowMaxSpeedHighlight)) {
						save = true;
					}
					ImGui::BeginDisabled(!BaseOverlay::ShowMaxSpeedHighlight);
					if(ImGui::InputFloat(TR(HIGHLIGHT_TRESHOLD), &BaseOverlay::MaxSpeedPerSecond)) {
						save = true;
					}
					ImGui::ColorEdit3(TR_ID("HighlightColor", Tr::COLOR), &BaseOverlay::MaxSpeedColor.Value.x, ImGuiColorEditFlags_None);
					if(ImGui::IsItemDeactivatedAfterEdit()) {
						save = true;
					}
					ImGui::EndDisabled();
					
					ImGui::Separator();
					if (ImGui::InputInt(TR(FAST_FRAME_STEP), &scripterSettings.fastStepAmount, 1, 1)) {
						save = true;
						scripterSettings.fastStepAmount = Util::Clamp<int32_t>(scripterSettings.fastStepAmount, 2, 30);
					}
					OFS::Tooltip(TR(FAST_FRAME_STEP_TOOLTIP));
					ImGui::Separator();
					if (ImGui::Checkbox(TR(SHOW_METADATA_DIALOG_ON_NEW_PROJECT), &scripterSettings.showMetaOnNew)) {
						save = true;
					}
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
			ImGui::EndChild();
		}
		ImGui::EndPopup();
	}
	return save;
}

void OFS_Settings::SetTheme(OFS_Theme theme) noexcept
{
	auto& style = ImGui::GetStyle();
	auto& io = ImGui::GetIO();

	switch (theme) {
		case OFS_Theme::Dark:
		{
			ImGui::StyleColorsDark(&style);
			break;
		}
		case OFS_Theme::Light:
		{
			ImGui::StyleColorsLight(&style);
			break;
		}
	}

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		//style.WindowRounding = 0.0f;
		style.WindowRounding = 6.f;
		style.Colors[ImGuiCol_WindowBg].w = 1.f;
		style.Colors[ImGuiCol_PopupBg].w = 1.f;
	}
}
