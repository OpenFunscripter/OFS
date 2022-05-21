#include "OFS_Settings.h"
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
		configObj = Util::LoadJson(config, &success);
		if (!success) {
			LOGF_ERROR("Failed to parse config @ \"%s\"", config.c_str());
			configObj.clear();
		}
	}

	if (!configObj[ConfigStr].is_object())
		configObj[ConfigStr] = nlohmann::json::object();
	else
		load_config();
}

void OFS_Settings::save_config()
{
	Util::WriteJson(configObj, config_path, true);
}

void OFS_Settings::load_config()
{
	OFS::serializer::load(&scripterSettings, &config());
}

void OFS_Settings::saveSettings()
{
	OFS::serializer::save(&scripterSettings, &config());
	save_config();
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
					if (ImGui::RadioButton(TR(DARK_MODE), (int*)&scripterSettings.current_theme,
						(uint8_t)OFS_Theme::dark)) {
						SetTheme(scripterSettings.current_theme);
						save = true;
					}
					ImGui::SameLine();
					if (ImGui::RadioButton(TR(LIGHT_MODE), (int*)&scripterSettings.current_theme,
						(uint8_t)OFS_Theme::light)) {
						SetTheme(scripterSettings.current_theme);
						save = true;
					}
					
					ImGui::Separator();

					ImGui::TextWrapped(TR(PREFERENCES_TXT));
					if (ImGui::Checkbox(TR(VSYNC), (bool*)&scripterSettings.vsync)) {
						scripterSettings.vsync = Util::Clamp(scripterSettings.vsync, 0, 1); // just in case...
						SDL_GL_SetSwapInterval(scripterSettings.vsync);
						save = true;
					}
					OFS::Tooltip(TR(VSYNC_TOOLTIP));
					ImGui::SameLine();
					if (ImGui::InputInt(TR(FRAME_LIMIT), &scripterSettings.framerateLimit, 1, 10)) {
						scripterSettings.framerateLimit = Util::Clamp(scripterSettings.framerateLimit, 60, 300);
						save = true;
					}
					OFS::Tooltip(TR(FRAME_LIMIT_TOOLTIP));
					ImGui::Separator();
					ImGui::InputText(TR(FONT), scripterSettings.font_override.empty() ? (char*)TR(DEFAULT_FONT) : (char*)scripterSettings.font_override.c_str(),
						scripterSettings.font_override.size(), ImGuiInputTextFlags_ReadOnly);
					ImGui::SameLine();
					if (ImGui::Button(TR(CHANGE))) {
						Util::OpenFileDialog(TR(CHOOSE_FONT), "",
							[&](auto& result) {
								if (result.files.size() > 0) {
									scripterSettings.font_override = result.files.back();
									OpenFunscripter::ptr->LoadOverrideFont(scripterSettings.font_override);
									save = true;
								}
							}, false, { "*.ttf", "*.otf" }, "Fonts (*.ttf, *.otf)");
					}
					ImGui::SameLine();
					if (ImGui::Button(TR(CLEAR))) {
						scripterSettings.font_override = "";
						EventSystem::SingleShot([](void* ctx) {
							// fonts can't be updated during a frame
							// this updates the font during event processing
							// which is not during the frame
							auto app = OpenFunscripter::ptr;
							app->LoadOverrideFont(app->settings->data().font_override);
							}, nullptr);
					}

					if (ImGui::InputInt(TR(FONT_SIZE), (int*)&scripterSettings.default_font_size, 1, 1)) {
						scripterSettings.default_font_size = Util::Clamp(scripterSettings.default_font_size, 8, 64);
						EventSystem::SingleShot([](void* ctx) {
							// fonts can't be updated during a frame
							// this updates the font during event processing
							// which is not during the frame
							auto app = OpenFunscripter::ptr;
							app->LoadOverrideFont(app->settings->data().font_override);
							}, nullptr);
						save = true;
					}
					if(ImGui::BeginCombo(TR_ID("LANGUAGE", Tr::LANGUAGE), data().language_csv.empty() ? "English" : data().language_csv.c_str()))
					{
						for(auto& file : translationFiles)
						{
							if(ImGui::Selectable(file.c_str(), file == data().language_csv))
							{
								if(OFS_Translator::ptr->LoadTranslation(file.c_str()))
								{
									data().language_csv = file;
									OFS_DynFontAtlas::AddTranslationText();
								}
							}
						}
						ImGui::EndCombo();
					}
					if(ImGui::IsItemClicked(ImGuiMouseButton_Left))
					{
						translationFiles.clear();
						std::error_code ec;
						std::filesystem::directory_iterator dirIt(Util::Prefpath(OFS_Translator::TranslationDir), ec);
						for (auto&& pIt : dirIt) {
							if(pIt.path().extension() == ".csv")
							{
								translationFiles.emplace_back(pIt.path().filename().u8string());
							}
						}
					}
					ImGui::SameLine();
					if(ImGui::Button(TR(RESET))) {
						data().language_csv = std::string();
						OFS_Translator::ptr->LoadDefaults();
					}
					ImGui::SameLine();
					if(ImGui::Button(TR_ID("OPEN_DIR_LANG", Tr::DIRECTORY)))
					{
						Util::OpenFileExplorer(Util::Prefpath(OFS_Translator::TranslationDir));
					}
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(TR(VIDEOPLAYER))) {
					if (ImGui::Checkbox(TR(FORCE_HW_DECODING), &scripterSettings.force_hw_decoding)) {
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
					if (ImGui::InputInt(TR(FAST_FRAME_STEP), &scripterSettings.fast_step_amount, 1, 1)) {
						save = true;
						scripterSettings.fast_step_amount = Util::Clamp<int32_t>(scripterSettings.fast_step_amount, 2, 30);
					}
					OFS::Tooltip(TR(FAST_FRAME_STEP_TOOLTIP));
					ImGui::Separator();
					if (ImGui::Checkbox(TR(SHOW_METADATA_DIALOG_ON_NEW_PROJECT), &scripterSettings.show_meta_on_new)) {
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
		case OFS_Theme::dark:
		{
			ImGui::StyleColorsDark(&style);
			break;
		}
		case OFS_Theme::light:
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
