#include "OFP_Videobrowser.h"

#include "OFS_Util.h"
#include "EventSystem.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "SDL_thread.h"
#include "SDL_atomic.h"
#include "SDL_timer.h"

#include <filesystem>
#include <sstream>

#ifdef WIN32
//used to obtain drives on windows
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

SDL_sem* Videobrowser::ThumbnailThreadSem = nullptr;

static uint64_t GetFileAge(const std::filesystem::path& path) {
	std::error_code ec;
	uint64_t timestamp = 0;

#ifdef WIN32
	HANDLE file = CreateFileW((wchar_t*)path.u16string().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		LOGF_ERROR("Could not open file \"%s\", error 0x%08x", path.u8string().c_str(), GetLastError());
	}
	else {
		FILETIME ftCreate;
		if (!GetFileTime(file, &ftCreate, NULL, NULL))
		{
			LOG_ERROR("Couldn't GetFileTime");
			timestamp = std::filesystem::last_write_time(path, ec).time_since_epoch().count();
		}
		else {
			timestamp = *(uint64_t*)&ftCreate;
		}
		CloseHandle(file);
	}
#else
	timestamp = std::filesystem::last_write_time(path, ec).time_since_epoch().count();
#endif
	return timestamp;
}

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

		SDL_AtomicLock(&browser.ItemsLock);
		browser.Items.emplace_back((pathObj / "..").u8string(), 0, 0, false, false );
		SDL_AtomicUnlock(&browser.ItemsLock);

		std::error_code ec;
		for (auto& p : std::filesystem::directory_iterator(pathObj, ec)) {
			auto extension = p.path().extension().u8string();
			auto pathString = p.path().u8string();

			auto it = std::find_if(BrowserExtensions.begin(), BrowserExtensions.end(),
				[&](auto& ext) {
					return std::strcmp(ext.first, extension.c_str()) == 0;
			});
		
			if (it != BrowserExtensions.end()) {
				auto timestamp = GetFileAge(p.path());
				auto funscript = p.path();
				funscript.replace_extension(".funscript");
				bool matchingScript = Util::FileExists(funscript.u8string());
				// valid extension + matching script
				size_t byte_count = p.file_size();
				SDL_AtomicLock(&browser.ItemsLock);
				browser.Items.emplace_back(p.path().u8string(), byte_count, timestamp, it->second && browser.settings->showThumbnails, matchingScript);
				SDL_AtomicUnlock(&browser.ItemsLock);
			}
			else if (p.is_directory()) {
				SDL_AtomicLock(&browser.ItemsLock);
				browser.Items.emplace_back(p.path().u8string(), 0, 0, false, false);
				SDL_AtomicUnlock(&browser.ItemsLock);
			}
		}
		SDL_AtomicLock(&browser.ItemsLock);
		std::sort(browser.Items.begin(), browser.Items.end(), [](auto& item1, auto& item2) {
			return item1.IsDirectory() && !item2.IsDirectory();
		});
		auto last_dir = std::find_if(browser.Items.begin(), browser.Items.end(),
			[](auto& item) {
				return !item.IsDirectory();
		});
		if (last_dir != browser.Items.end()) {
			std::sort(last_dir, browser.Items.end(),
				[](auto& item1, auto& item2) {
					return item1.lastEdit > item2.lastEdit;
				}
			);
		}
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

void Videobrowser::updateLibraryCache() noexcept
{
	struct UpdateLibraryThreadData {
		Videobrowser* browser;
	};

	auto updateLibrary = [](void* ctx) -> int {
		auto data = static_cast<UpdateLibraryThreadData*>(ctx);
		auto& browser = *data->browser;

		SDL_AtomicLock(&browser.ItemsLock);
		browser.Items.clear();
		LOG_DEBUG("ITEMS CLEAR");
		SDL_AtomicUnlock(&browser.ItemsLock);

		auto& cache = browser.libCache;
		cache.videos.clear();

		auto& searchPaths = data->browser->settings->SearchPaths;
		for (auto& sPath : searchPaths) {
			auto pathObj = Util::PathFromString(sPath.path);

			auto handleItem = [&](auto& p) {
				auto extension = p.path().extension().u8string();
				auto pathString = p.path().u8string();
				if (p.is_directory()) return;

				auto it = std::find_if(BrowserExtensions.begin(), BrowserExtensions.end(),
					[&](auto& ext) {
						return std::strcmp(ext.first, extension.c_str()) == 0;
					});
				if (it != BrowserExtensions.end()) {
					// extension matches
					auto timestamp = GetFileAge(p.path());
					auto funscript = p.path();
					funscript.replace_extension(".funscript");
					bool matchingScript = Util::FileExists(funscript.u8string());
					if (!matchingScript) return;

					// valid extension + matching script
					size_t byte_count = p.file_size();

					LibraryCachedVideos::CachedVideo vid;
					vid.path = p.path().u8string();
					vid.byte_count = byte_count;
					vid.thumbnail = it->second;
					vid.hasScript = matchingScript;
					vid.timestamp = timestamp;

					SDL_AtomicLock(&browser.ItemsLock);
					browser.Items.emplace_back(vid.path, vid.byte_count, timestamp, vid.thumbnail && browser.settings->showThumbnails, vid.hasScript);
					SDL_AtomicUnlock(&browser.ItemsLock);

					cache.videos.emplace_back(std::move(vid));
				}
			};

			std::error_code ec;
			if (sPath.recursive) {
				for (auto& p : std::filesystem::recursive_directory_iterator(pathObj, ec)) {
					handleItem(p);
				}
			}
			else {
				for (auto& p : std::filesystem::directory_iterator(pathObj, ec)) {
					handleItem(p);
				}
			}
			LOGF_DEBUG("Done iterating \"%s\" %s", sPath.path.c_str(), sPath.recursive ? "recursively" : "");
		}

		SDL_AtomicLock(&browser.ItemsLock);
		std::sort(browser.Items.begin(), browser.Items.end(),
			[](auto& item1, auto& item2) {
				return item1.lastEdit > item2.lastEdit;
			}
		);
		SDL_AtomicUnlock(&browser.ItemsLock);


		delete data;
		return 0;
	};

	CacheNeedsUpdate = false;
	auto data = new UpdateLibraryThreadData;
	data->browser = this;
	auto thread = SDL_CreateThread(updateLibrary, "UpdateVideoLibrary", data);
	SDL_DetachThread(thread);
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
			Items.emplace_back(ss.str(), 0, 0, false, false);
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
	libCache.load(Util::PrefpathOFP("library.json"));
	CacheNeedsUpdate = libCache.videos.empty();

	preview.setup();

	SDL_AtomicLock(&ItemsLock);
	for (auto& cached : libCache.videos) {
		Items.emplace_back(cached.path, cached.byte_count, cached.timestamp, cached.thumbnail, cached.hasScript);
	}
	SDL_AtomicUnlock(&ItemsLock);

#ifndef NDEBUG
	// foo
	{
		auto storage = Videolibrary::Storage();
		auto vids = storage.get_all<Video>();
		for (auto& vid : vids) {
			LOGF_DEBUG("%s in category", vid.filename.c_str());
		}

		storage.remove_all<Video>();

		Video vid;
		vid.path = "C:\\\\homework\porn.mp4";
		vid.filename = "porn.mp4";
		vid.byte_count = 123;
		vid.timestamp = 0;
		vid.insert();

		//auto vidId = storage.insert(vid);
	}
#endif
}

Videobrowser::~Videobrowser()
{
	libCache.save();
}

void Videobrowser::Lootcrate(bool* open) noexcept
{
	if (open != nullptr && !*open) return;

	if (Random)
		ImGui::OpenPopup(VideobrowserRandomId);

	if (ImGui::BeginPopupModal(VideobrowserRandomId, open, ImGuiWindowFlags_None)) {
		auto hostWidth = ImGui::GetContentRegionAvail().x;
		ImGui::BeginChild("RandomVideo", ImVec2(hostWidth, hostWidth), true);
		renderLoot();
		ImGui::EndChild();
		ImGui::Button("Spin!", ImVec2(-1, 0));
		ImGui::EndPopup();
	}
}

void Videobrowser::renderLoot() noexcept
{
	if (Items.empty()) return;

	auto window_pos = ImGui::GetWindowPos();
	auto availSize = ImGui::GetContentRegionAvail();
	
	auto window = ImGui::GetCurrentWindowRead();
	
	auto frame_bb = ImRect(window->DC.CursorPos, window->DC.CursorPos +availSize);

	auto draw_list = ImGui::GetWindowDrawList();

	auto style = ImGui::GetStyle();

	constexpr float scale = 4.f;


	ImVec2 videoSize((availSize.x / scale) * (16.f / 9.f), (availSize.x / scale));
	
	static ImVec2 offset(0.f, 0.f);
	offset.x = std::sin(SDL_GetTicks() / 1500.f)*videoSize.x*5.f;

	ImVec2 centerPos(frame_bb.GetWidth() / 2.f, frame_bb.GetHeight() / 2.f);
	centerPos += frame_bb.Min;
	

	ImVec2 wheelPos = centerPos + offset;
	ImVec2 p1 = wheelPos - (videoSize / 2.f);
	ImVec2 p2 = wheelPos + (videoSize / 2.f);
	draw_list->AddRect(p1, p2, IM_COL32(255, 0, 0, 255), 2.f);
	//draw_list->AddImage((void*)(intptr_t)*Items.begin()->texture.GetTexId(), p1 + style.FramePadding, p2 - style.FramePadding, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE);

	for (int i = 1; i <= 10; i++) {
		wheelPos += ImVec2(videoSize.x + style.ItemSpacing.x, 0.f);
		p1 = wheelPos - (videoSize / 2.f);
		p2 = wheelPos + (videoSize / 2.f);
		draw_list->AddRect(p1, p2, IM_COL32(255, 0, 0, 255), 2.f);
		//draw_list->AddImage((void*)(intptr_t)*(Items.begin()+i)->texture.GetTexId(), p1 + style.FramePadding, p2 - style.FramePadding, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE);
	}

	draw_list->AddLine(centerPos - ImVec2(0.f, availSize.x / 4.f), centerPos + ImVec2(0.f, availSize.x / 4.f), IM_COL32(255, 255, 0, 255));
}

void Videobrowser::ShowBrowser(const char* Id, bool* open) noexcept
{
	if (open != NULL && !*open) return;
	if (CacheNeedsUpdate) { updateLibraryCache(); /*updateCache(settings->CurrentPath);*/ }

	uint32_t window_flags = 0;
	window_flags |= ImGuiWindowFlags_MenuBar; 

	SDL_AtomicLock(&ItemsLock);
	ImGui::Begin(Id, open, window_flags);
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("View")) {
			ImGui::MenuItem("Show thumbnails", NULL, &settings->showThumbnails);
			Util::Tooltip("Requires reload.");
			ImGui::SetNextItemWidth(ImGui::GetFontSize()*5.f);
			ImGui::InputInt("Items", &settings->ItemsPerRow, 1, 10);
			settings->ItemsPerRow = Util::Clamp(settings->ItemsPerRow, 1, 25);
			ImGui::EndMenu();
		}
		ImGui::MenuItem("Settings", NULL, &ShowSettings);
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
	
	ImGui::BeginChild("Items", ImVec2(0,0), true);
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
	


	VideobrowserItem* previewItem = nullptr;

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

			ImColor FileTint = item.HasMatchingScript ? IM_COL32_WHITE : IM_COL32(200, 200, 200, 255);
			if (!item.Focussed) {
				FileTint.Value.x *= 0.75f;
				FileTint.Value.y *= 0.75f;
				FileTint.Value.z *= 0.75f;
			}

			auto texId = item.texture.GetTexId();
			if (texId != 0) {
				ImVec2 padding = item.HasMatchingScript ? ImVec2(0, 0) : ImVec2(ItemWidth*0.1f, ItemWidth*0.1f);
				ImGui::PushID(index);
				if(ImGui::ImageButtonEx(ImGui::GetID(item.filename.c_str()), item.Focussed && preview.ready ? (void*)(intptr_t)preview.render_texture : (void*)(intptr_t)texId, ItemDim - padding, ImVec2(0, 0), ImVec2(1, 1), padding/2.f, style.Colors[ImGuiCol_PlotLines], FileTint)) {
					fileClickHandler(item);
				}
				ImGui::PopID();
				bool prevValue = item.Focussed;
				item.Focussed = /*ImGui::IsItemActive() || ImGui::IsItemActivated() ||*/ ImGui::IsItemFocused() || ImGui::IsItemHovered();
				if (item.Focussed) {
					previewItem = &item;
				}
			}
			else {
				if(!item.HasMatchingScript) { ImGui::PushStyleColor(ImGuiCol_Button, FileTint.Value); }
				
				if (ImGui::Button(item.filename.c_str(), ItemDim)) {
					fileClickHandler(item);
				}
				if (item.HasThumbnail && ImGui::IsItemVisible()) {
					item.GenThumbail();
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

	if (previewItem != nullptr && !preview.loading || previewItem != nullptr && previewItem->texture.Id != previewItemId) {
		previewItemId = previewItem->texture.Id;
		preview.previewVideo(previewItem->path, 0.2f);
	}
	else if (previewItem == nullptr && preview.ready) {
		preview.closeVideo();
	}

	ImGui::EndChild();
	ImGui::End();
	SDL_AtomicUnlock(&ItemsLock);

	ShowBrowserSettings(&ShowSettings);

	SDL_AtomicLock(&ItemsLock);
	Lootcrate(&Random);
	SDL_AtomicUnlock(&ItemsLock);
}

void Videobrowser::ShowBrowserSettings(bool* open) noexcept
{
	if (open != nullptr && !*open) return;
	
	if (ShowSettings)
		ImGui::OpenPopup(VideobrowserSettingsId);

	if (ImGui::BeginPopupModal(VideobrowserSettingsId, open, ImGuiWindowFlags_AlwaysAutoResize)) {

		if (ImGui::BeginTable("##SearchPaths", 3, ImGuiTableFlags_Borders)) {
			int32_t index = 0;
			for (index = 0; index < settings->SearchPaths.size(); index++) {
				ImGui::PushID(index);
				auto& search = settings->SearchPaths[index];
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(search.path.c_str()); 
				Util::Tooltip(search.path.c_str());

				ImGui::TableNextColumn();
				ImGui::Checkbox("Recursive", &search.recursive);

				ImGui::TableNextColumn();
				if (ImGui::Button("Remove", ImVec2(-1, 0))) {
					ImGui::PopID();
					break;
				}
				ImGui::PopID();
			}
			if (index < settings->SearchPaths.size()) {
				settings->SearchPaths.erase(settings->SearchPaths.begin() + index);
			}
			ImGui::EndTable();
		}

		if (ImGui::Button("Choose path", ImVec2(-1, 0)))
		{
			Util::OpenDirectoryDialog("Choose search path", "", [&](auto& result) {
				if (result.files.size() > 0) {
					VideobrowserSettings::LibraryPath path;
					path.path = result.files.front();
					path.recursive = false;
					settings->SearchPaths.emplace_back(std::move(path));
				}
			});
		}

		ImGui::EndPopup();
	}
}

int32_t VideobrowserEvents::VideobrowserItemClicked = 0;

void VideobrowserEvents::RegisterEvents() noexcept
{
	VideobrowserItemClicked = SDL_RegisterEvents(1);
}

#include "OFS_Serialization.h"
#include "nlohmann/json.hpp"

void LibraryCachedVideos::load(const std::string& path) noexcept
{
	cachePath = path;
	bool succ;
	auto json = Util::LoadJson(path, &succ);
	if (succ) {
		OFS::serializer::load(this, &json);
	}
}

void LibraryCachedVideos::save() noexcept
{
	nlohmann::json json;
	OFS::serializer::save(this, &json);
	Util::WriteJson(json, cachePath, false);
}