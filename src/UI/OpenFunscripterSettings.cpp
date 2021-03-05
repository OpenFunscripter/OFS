#include "OpenFunscripterSettings.h"
#include "OFS_Util.h"
#include "OpenFunscripter.h"

#include "OFS_Serialization.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>


OpenFunscripterSettings::OpenFunscripterSettings(const std::string& config)
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

	scripterSettings.simulator = &OpenFunscripter::ptr->simulator.simulator;	
	if (!configObj[ConfigStr].is_object())
		configObj[ConfigStr] = nlohmann::json::object();
	else
		load_config();
}

void OpenFunscripterSettings::save_config()
{
	Util::WriteJson(configObj, config_path, true);
}

void OpenFunscripterSettings::load_config()
{
	OFS::serializer::load(&scripterSettings, &config());
}

void OpenFunscripterSettings::saveSettings()
{
	OFS::serializer::save(&scripterSettings, &config());
	save_config();
}

bool OpenFunscripterSettings::ShowPreferenceWindow()
{
	bool save = false;
	if (ShowWindow)
		ImGui::OpenPopup("Preferences");

	if (ImGui::BeginPopupModal("Preferences", &ShowWindow, ImGuiWindowFlags_None | ImGuiWindowFlags_AlwaysAutoResize))
	{
		OFS_PROFILE(__FUNCTION__);

		ImGui::InputText("Font", scripterSettings.font_override.empty() ? (char*)"Default font" : (char*)scripterSettings.font_override.c_str(),
			scripterSettings.font_override.size(), ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		if (ImGui::Button("Change")) {
			Util::OpenFileDialog("Choose font", "",
				[&](auto& result) {
					if (result.files.size() > 0) {
						scripterSettings.font_override = result.files.back();
						if (!OpenFunscripter::ptr->LoadOverrideFont(scripterSettings.font_override)) {
							scripterSettings.font_override = "";
						}
						else {
							save = true;
						}
					}
				}, false, { "*.ttf", "*.otf" }, "Fonts (*.ttf, *.otf)");
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear")) {
			scripterSettings.font_override = "";
			EventSystem::SingleShot([](void* ctx) {
				// fonts can't be updated during a frame
				// this updates the font during event processing
				// which is not during the frame
				auto app = OpenFunscripter::ptr;
				app->LoadOverrideFont(app->settings->data().font_override);
			}, nullptr);
		}
		
		if (ImGui::InputInt("Font size", (int*)&scripterSettings.default_font_size, 1, 1)) {
			scripterSettings.default_font_size = Util::Clamp(scripterSettings.default_font_size, 16, 48);
			EventSystem::SingleShot([](void* ctx) {
				// fonts can't be updated during a frame
				// this updates the font during event processing
				// which is not during the frame
				auto app = OpenFunscripter::ptr;
				app->LoadOverrideFont(app->settings->data().font_override);
			}, nullptr);
			save = true;
		}

		if (ImGui::Checkbox("Vsync", (bool*)&scripterSettings.vsync)) {
			scripterSettings.vsync = Util::Clamp(scripterSettings.vsync, 0, 1); // just in case...
			SDL_GL_SetSwapInterval(scripterSettings.vsync);
			save = true;
		}
		ImGui::SameLine();
		if (ImGui::InputInt("Frame limit", &scripterSettings.framerateLimit, 1, 10)) {
			scripterSettings.framerateLimit = Util::Clamp(scripterSettings.framerateLimit, 30, 300);
			save = true;
		}
		if (ImGui::Checkbox("Force hardware decoding (Requires program restart)", &scripterSettings.force_hw_decoding)) {
			save = true;
		}
		Util::Tooltip("Use this for really high resolution video 4K+ VR videos for example.");

		if (ImGui::InputInt("Fast frame step", &scripterSettings.fast_step_amount, 1, 1)) {
			save = true;
			scripterSettings.fast_step_amount = Util::Clamp<int32_t>(scripterSettings.fast_step_amount, 2, 30);
		}
		Util::Tooltip("Amount of frames to skip with fast step.");

		ImGui::EndPopup();
	}

	return save;
}