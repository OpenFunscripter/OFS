#include "OFP_Videobrowser.h"

#include "OFS_Util.h"
#include "EventSystem.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "SDL_thread.h"
#include "SDL_atomic.h"

#include <filesystem>
#include <sstream>

#ifdef WIN32
//used to obtain drives on windows
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

SDL_sem* Videobrowser::ThumbnailThreadSem = nullptr;

void Videobrowser::updateCache(const std::string& path) noexcept
{
	static struct IterateDirData {
		std::string path;
		bool running = false;
		Videobrowser* browser;
	} IterateThread;
	if (IterateThread.running) return;

	auto iterateDirThread = [](void* ctx) -> int {
		IterateDirData* data = (IterateDirData*)ctx;
		Videobrowser& browser = *data->browser;
		
		SDL_AtomicLock(&browser.ItemsLock);
		browser.Items.clear();
		LOG_DEBUG("ITEMS CLEAR");
		SDL_AtomicUnlock(&browser.ItemsLock);

		auto pathObj = Util::PathFromString(data->path);
		pathObj = std::filesystem::absolute(pathObj);
		std::error_code ec;
		for (auto& p : std::filesystem::directory_iterator(pathObj, ec)) {
			auto extension = p.path().extension().u8string();


			auto it = std::find_if(BrowserExtensions.begin(), BrowserExtensions.end(),
				[&](auto& ext) {
					return std::strcmp(ext.first, extension.c_str()) == 0;
			});
		
			if (it != BrowserExtensions.end()) {
				auto funscript = p.path();
				funscript.replace_extension(".funscript");
				bool matchingScript = Util::FileExists(funscript.u8string());
				// valid extension + matching script
				size_t byte_count = p.file_size();
				SDL_AtomicLock(&browser.ItemsLock);
				browser.Items.emplace_back(p.path().u8string(), byte_count, it->second, matchingScript);
				SDL_AtomicUnlock(&browser.ItemsLock);
			}
			else if (p.is_directory()) {
				SDL_AtomicLock(&browser.ItemsLock);
				browser.Items.emplace_back(p.path().u8string(), 0, false, false);
				SDL_AtomicUnlock(&browser.ItemsLock);
			}
		}
		SDL_AtomicLock(&browser.ItemsLock);
		std::sort(browser.Items.begin(), browser.Items.end(),
			[](auto& item1, auto& item2) {
				return item1.filename < item2.filename;
			}
		);
		std::sort(browser.Items.begin(), browser.Items.end(), [](auto& item1, auto& item2) {
			return item1.IsDirectory() && !item2.IsDirectory();
		});
		browser.Items.insert(browser.Items.begin(), {(pathObj / "..").u8string(), 0, false, false });
		SDL_AtomicUnlock(&browser.ItemsLock);

		data->running = false;
		return 0;
	};

	IterateThread.running = true;
	IterateThread.path = path;
	IterateThread.browser = this;
	auto thread = SDL_CreateThread(iterateDirThread, "IterateDirectory", &IterateThread);
	SDL_DetachThread(thread);
	CacheNeedsUpdate = false;
}

#ifdef WIN32
void Videobrowser::chooseDrive() noexcept
{
	Items.clear();
	auto AvailableDrives = GetLogicalDrives();
	uint32_t Mask = 0b1;
	std::stringstream ss;
	for (int i = 0; i < 32; i++) {
		if (AvailableDrives & (Mask << i)) {
			ss << (char)('A' + i);
			ss << ":\\\\";
			Items.emplace_back(ss.str(), 0, false, false);
			ss.str("");
		}
	}
}
#endif

Videobrowser::Videobrowser(VideobrowserSettings* settings)
	: settings(settings)
{
	if (ThumbnailThreadSem == nullptr) {
		ThumbnailThreadSem = SDL_CreateSemaphore(MaxThumbailProcesses);
	}
}

void Videobrowser::ShowBrowser(const char* Id, bool* open) noexcept
{
	if (open != NULL && !*open) return;
	if (CacheNeedsUpdate) { updateCache(settings->CurrentPath); }

	uint32_t window_flags = 0;
	window_flags |= ImGuiWindowFlags_MenuBar; 

	SDL_AtomicLock(&ItemsLock);
	ImGui::Begin(Id, open, window_flags);
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("View")) {
			ImGui::SetNextItemWidth(ImGui::GetFontSize()*5.f);
			ImGui::InputInt("Items", &settings->ItemsPerRow, 1, 10);
			settings->ItemsPerRow = Util::Clamp(settings->ItemsPerRow, 1, 25);
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	if (ImGui::Button(ICON_REFRESH)) {
		CacheNeedsUpdate = true;
	}
	ImGui::SameLine();
	ImGui::Bullet();
	ImGui::TextUnformatted(settings->CurrentPath.c_str());
	ImGui::Separator();
	
	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputText("Filter", &Filter);

	auto availSpace = ImGui::GetContentRegionMax();
	auto& style = ImGui::GetStyle();

	float ItemWidth = (availSpace.x - (2.f * style.ItemInnerSpacing.x) - (settings->ItemsPerRow*style.ItemSpacing.x)) / (float)settings->ItemsPerRow;
	ItemWidth = std::max(ItemWidth, 2.f);
	const float ItemHeight = (9.f/16.f)*ItemWidth;
	const auto ItemDim = ImVec2(ItemWidth, ItemHeight);
	
	ImGui::BeginChild("Items");
	auto fileClickHandler = [&](VideobrowserItem& item) {		
		if (item.HasMatchingScript) {
			ClickedFilePath = item.path;
			EventSystem::PushEvent(VideobrowserEvents::VideobrowserItemClicked);
		}
	};

	auto directoryClickHandler = [&](VideobrowserItem& item) {
#ifdef WIN32
		auto pathObj = Util::PathFromString(item.path);
		auto pathObjAbs = std::filesystem::absolute(pathObj);
		if (pathObj != pathObjAbs && pathObj.root_path() == pathObjAbs) {
			chooseDrive();
		}
		else
#endif
		{
			settings->CurrentPath = item.path;
			CacheNeedsUpdate = true;
			// this ensures the items are focussed
			ImGui::SetFocusID(ImGui::GetID(".."), ImGui::GetCurrentWindow());
		}
	};

	if (ImGui::IsNavInputTest(ImGuiNavInput_Cancel, ImGuiInputReadMode_Pressed)) {
		// go up one directory
		// this assumes Items.front() contains ".."
		if (Items.size() > 0 && Items.front().filename == "..") {
			directoryClickHandler(Items.front());
		}
	}
	else if (ImGui::IsNavInputTest(ImGuiNavInput_FocusPrev, ImGuiInputReadMode_Pressed))
	{
		settings->ItemsPerRow--;
		settings->ItemsPerRow = Util::Clamp(settings->ItemsPerRow, 1, 25);
	}
	else if (ImGui::IsNavInputTest(ImGuiNavInput_FocusNext, ImGuiInputReadMode_Pressed)) 
	{
		settings->ItemsPerRow++;
		settings->ItemsPerRow = Util::Clamp(settings->ItemsPerRow, 1, 25);
	}
	

	int index = 0;
	for (auto& item : Items) {
		if (index != 0 && !Filter.empty()) {
			if (!Util::ContainsInsensitive(item.filename, Filter)) {
				continue;
			}
		}
		if (!item.IsDirectory()) {
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_PlotLinesHovered]);
			ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_PlotLines]);

			uint32_t texId = item.GetTexId();
			ImColor FileTint = item.HasMatchingScript ? IM_COL32_WHITE : IM_COL32(200, 200, 200, 255);
			if (!item.Focussed) {
				FileTint.Value.x *= 0.75f;
				FileTint.Value.y *= 0.75f;
				FileTint.Value.z *= 0.75f;
			}
			if (texId != 0) {
				ImVec2 padding = item.HasMatchingScript ? ImVec2(0, 0) : ImVec2(ItemWidth*0.1f, ItemWidth*0.1f);
				if (ImGui::ImageButton((void*)(intptr_t)texId, ItemDim - padding, ImVec2(0,0), ImVec2(1,1), padding.x/2.f, ImVec4(0,0,0,0), FileTint)) {
					fileClickHandler(item);
				}
				item.Focussed = ImGui::IsItemFocused() || ImGui::IsItemHovered();
			}
			else {
				if(!item.HasMatchingScript) { ImGui::PushStyleColor(ImGuiCol_Button, FileTint.Value); }
				
				if (ImGui::Button(item.filename.c_str(), ItemDim)) {
					fileClickHandler(item);
				}
				if (!item.HasMatchingScript) { ImGui::PopStyleColor(1); }
			}
			ImGui::PopStyleColor(2);
		}
		else {
			if (ImGui::Button(item.filename.c_str(), ItemDim)) {
				directoryClickHandler(item);
			}
		}
		Util::Tooltip(item.filename.c_str());
		index++;
		if (index % settings->ItemsPerRow != 0) {
			ImGui::SameLine();
		}
	}

	ImGui::EndChild();
	ImGui::End();
	SDL_AtomicUnlock(&ItemsLock);
}

int32_t VideobrowserEvents::VideobrowserItemClicked = 0;

void VideobrowserEvents::RegisterEvents() noexcept
{
	VideobrowserItemClicked = SDL_RegisterEvents(1);
}