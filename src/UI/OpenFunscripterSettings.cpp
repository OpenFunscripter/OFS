#include "OpenFunscripterSettings.h"
#include "OpenFunscripterUtil.h"
#include "OpenFunscripter.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>


OpenFunscripterSettings::OpenFunscripterSettings(const std::string& keybinds, const std::string& config)
	: keybinds_path(keybinds), config_path(config)
{
	if (Util::FileExists(keybinds)) {
		keybindsObj = Util::LoadJson(keybinds);
	}
	if (Util::FileExists(config)) {
		configObj = Util::LoadJson(config);
	}

	scripterSettings.simulator = &OpenFunscripter::ptr->simulator.simulator;
	if(!keybindsObj[KeybindsStr].is_array())
		keybindsObj[KeybindsStr] = nlohmann::json().array();
	
	if (!configObj[ConfigStr].is_object())
		configObj[ConfigStr] = nlohmann::json().object();
	else
		load_config();
}

void OpenFunscripterSettings::save_keybinds()
{
	Util::WriteJson(keybindsObj, keybinds_path, true);
}

void OpenFunscripterSettings::save_config()
{
	Util::WriteJson(configObj, config_path, true);
}

#define LOAD_CONFIG(member) if (config().contains( #member ))\
 {\
	auto ptr = config()[ #member ].get_ptr<decltype( ScripterSettingsData::member )*>();\
	if(ptr != nullptr) scripterSettings.member = *ptr;\
 }
void OpenFunscripterSettings::load_config()
{
	LOAD_CONFIG(last_path)
	LOAD_CONFIG(last_opened_video)
	LOAD_CONFIG(last_opened_script)
	LOAD_CONFIG(draw_video)
	LOAD_CONFIG(show_simulator)
	LOAD_CONFIG(force_hw_decoding)
	LOAD_CONFIG(always_show_bookmark_labels)
	LOAD_CONFIG(screenshot_dir)
	LOAD_CONFIG(default_font_size)

	auto& simulator = config()["simulator"];
	if (!simulator.empty()) {
		
		if (!simulator["Width"].empty()) {
			scripterSettings.simulator->Width = simulator["Width"].get<float>();
		}
		if (!simulator["BorderWidth"].empty()) {
			scripterSettings.simulator->BorderWidth = simulator["BorderWidth"].get<float>();
		}

		if (!simulator["P1"].empty()) {
			auto& p1 = simulator["P1"];
			if (p1["x"].is_number_float())
				scripterSettings.simulator->P1.x = p1["x"].get<float>();
			if (p1["y"].is_number_float())
				scripterSettings.simulator->P1.y = p1["y"].get<float>();
		}
		if (!simulator["P2"].empty()) {
			auto& p2 = simulator["P2"];
			if (p2["x"].is_number_float())
				scripterSettings.simulator->P2.x = p2["x"].get<float>();
			if (p2["y"].is_number_float())
				scripterSettings.simulator->P2.y = p2["y"].get<float>();
		}


		auto& front = simulator["Front"];
		if (!front.empty()) {
			if (front["r"].is_number_float()) { scripterSettings.simulator->Front.Value.x = front["r"].get<float>(); }
			if (front["g"].is_number_float()) { scripterSettings.simulator->Front.Value.y = front["g"].get<float>(); }
			if (front["b"].is_number_float()) { scripterSettings.simulator->Front.Value.z = front["b"].get<float>(); }
			if (front["a"].is_number_float()) { scripterSettings.simulator->Front.Value.w = front["a"].get<float>(); }
		}

		auto& border = simulator["Border"];
		if (!border.empty()) {
			if (border["r"].is_number_float()) { scripterSettings.simulator->Border.Value.x = border["r"].get<float>(); }
			if (border["g"].is_number_float()) { scripterSettings.simulator->Border.Value.y = border["g"].get<float>(); }
			if (border["b"].is_number_float()) { scripterSettings.simulator->Border.Value.z = border["b"].get<float>(); }
			if (border["a"].is_number_float()) { scripterSettings.simulator->Border.Value.w = border["a"].get<float>(); }
		}

		auto& indicator = simulator["Indicator"];
		if (!front.empty()) {
			if (indicator["r"].is_number_float()) { scripterSettings.simulator->Indicator.Value.x = indicator["r"].get<float>(); }
			if (indicator["g"].is_number_float()) { scripterSettings.simulator->Indicator.Value.y = indicator["g"].get<float>(); }
			if (indicator["b"].is_number_float()) { scripterSettings.simulator->Indicator.Value.z = indicator["b"].get<float>(); }
			if (indicator["a"].is_number_float()) { scripterSettings.simulator->Indicator.Value.w = indicator["a"].get<float>(); }
		}

	}
}
#undef LOAD_CONFIG

#define SAVE_CONFIG(member) config()[ #member ] = scripterSettings.member;
void OpenFunscripterSettings::saveSettings()
{
	SAVE_CONFIG(last_path)
	SAVE_CONFIG(last_opened_video)
	SAVE_CONFIG(last_opened_script)
	SAVE_CONFIG(draw_video)
	SAVE_CONFIG(show_simulator)
	SAVE_CONFIG(force_hw_decoding)
	SAVE_CONFIG(always_show_bookmark_labels)
	SAVE_CONFIG(screenshot_dir)
	SAVE_CONFIG(default_font_size)

	auto& simulator = config()["simulator"];

	auto& p1 = simulator["P1"];
	p1["x"] = scripterSettings.simulator->P1.x;
	p1["y"] = scripterSettings.simulator->P1.y;

	auto& p2 = simulator["P2"];
	p2["x"] = scripterSettings.simulator->P2.x;
	p2["y"] = scripterSettings.simulator->P2.y;

	simulator["Width"] = scripterSettings.simulator->Width;
	simulator["BorderWidth"] = scripterSettings.simulator->BorderWidth;

	auto& front = simulator["Front"];
	front["r"] = scripterSettings.simulator->Front.Value.x;
	front["g"] = scripterSettings.simulator->Front.Value.y;
	front["b"] = scripterSettings.simulator->Front.Value.z;
	front["a"] = scripterSettings.simulator->Front.Value.w;
	
	auto& border = simulator["Border"];
	border["r"] = scripterSettings.simulator->Border.Value.x;
	border["g"] = scripterSettings.simulator->Border.Value.y;
	border["b"] = scripterSettings.simulator->Border.Value.z;
	border["a"] = scripterSettings.simulator->Border.Value.w;

	auto& indicator = simulator["Indicator"];
	indicator["r"] = scripterSettings.simulator->Indicator.Value.x;
	indicator["g"] = scripterSettings.simulator->Indicator.Value.y;
	indicator["b"] = scripterSettings.simulator->Indicator.Value.z;
	indicator["a"] = scripterSettings.simulator->Indicator.Value.w;

	save_config();
}
#undef SAVE_CONFIG

void OpenFunscripterSettings::saveKeybinds(const std::vector<Keybinding>& bindings)
{
	auto& keybinds = keybindsObj[KeybindsStr];
	for (auto& binding : bindings) {
		auto it = std::find_if(keybinds.begin(), keybinds.end(), 
			[&](auto& obj) { return obj["identifier"] == binding.identifier; }
		);
		nlohmann::json* ptr = nullptr;
		if (it != keybinds.end()) {
			auto& ref = *it;
			ref.clear();
			ptr = &ref;
		}
		else {
			nlohmann::json bindingObj;
			keybinds.push_back(bindingObj);
			ptr = &keybinds.back();
		}
		(*ptr)["identifier"] = binding.identifier;
		(*ptr)["ignore_repeats"] = binding.ignore_repeats;
		(*ptr)["key"] = binding.key;
		(*ptr)["modifiers"] = binding.modifiers;
	}
		
	save_keybinds();
}

std::vector<Keybinding> OpenFunscripterSettings::getKeybindings()
{
	auto& keybinds = keybindsObj[KeybindsStr];
	std::vector<Keybinding> bindings;
	bindings.reserve(keybinds.size());
	for (auto& bind : keybinds) {
		Keybinding b;
		if (bind.contains("ignore_repeats")
			&& bind.contains("identifier")
			&& bind.contains("key")
			&& bind.contains("modifiers")) {
			b.ignore_repeats = bind["ignore_repeats"];
			b.identifier = bind["identifier"].get<std::string>();
			b.key = bind["key"];
			b.modifiers = bind["modifiers"];
			bindings.push_back(b);
		}
	}

	return bindings;
}

bool OpenFunscripterSettings::ShowPreferenceWindow()
{
	bool save = false;
	if (ShowWindow)
		ImGui::OpenPopup("Preferences");

	if (ImGui::BeginPopupModal("Preferences", &ShowWindow, ImGuiWindowFlags_None | ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (ImGui::InputInt("Font size", (int*)&scripterSettings.default_font_size, 1, 1))
			save = true;
		Util::Tooltip("Requires program restart to take effect");
		if (ImGui::Checkbox("Force hardware decoding (Requires program restart)", &scripterSettings.force_hw_decoding))
			save = true;
		Util::Tooltip("Use this for really high resolution video 4K+ VR videos for example.");
		ImGui::EndPopup();
	}

	return save;
}