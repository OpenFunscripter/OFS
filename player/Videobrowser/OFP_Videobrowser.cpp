#include "OFP_Videobrowser.h"

#include "OFS_Util.h"
#include "EventSystem.h"
#include "OFS_ImGui.h"
#include "Funscript.h"
#include "OFP.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "SDL_thread.h"
#include "SDL_atomic.h"
#include "SDL_timer.h"

#include <filesystem>
#include <sstream>
#include <algorithm>
#include <future>

#ifdef WIN32
//used to obtain drives on windows
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

#ifndef NDEBUG
constexpr uint64_t MaxFileSearchFutures = 4;
#else
constexpr uint64_t MaxFileSearchFutures = 16;
#endif

SDL_sem* Videobrowser::ThumbnailThreadSem = SDL_CreateSemaphore(Videobrowser::MaxThumbailProcesses);

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

void Videobrowser::updateLibraryCache() noexcept
{
	CacheUpdateInProgress = true;
	CacheNeedsUpdate = false;
	struct UpdateLibraryThreadData {
		Videobrowser* browser;
	};

	auto updateLibrary = [](void* ctx) -> int {
		auto data = static_cast<UpdateLibraryThreadData*>(ctx);
		auto& browser = *data->browser;

		// clear current items
		{
			auto handle = EventSystem::WaitableSingleShot(
				[](void* ctx)
				{
					auto browser = (Videobrowser*)ctx;
					// this needs to run on the gl thread because
					//textures might be freed
					SDL_AtomicLock(&browser->ItemsLock);
					browser->Items.clear();
					SDL_AtomicUnlock(&browser->ItemsLock);
				}, &browser);
			handle->wait();
		}

		auto checkForRemovedFiles = []()
		{
			auto allVideos = Videolibrary::GetAll<Video>();
			for (auto& video : allVideos) {
				LOGF_INFO("Looking for %s", video.videoFilename.c_str());
				if (!Util::FileExists(video.videoPath))
				{
					Videolibrary::Remove<Video>(video.id);
					LOGF_INFO("Couldn't find %s", video.videoPath.c_str());
				}
				else if (!Util::FileExists(video.scriptPath))
				{
					Videolibrary::Remove<Video>(video.id);
					LOGF_INFO("Couldn't find %s", video.scriptPath.c_str());
				}
			}
		};

		auto searchPathsForVideos = [](UpdateLibraryThreadData* data)
		{
			auto& searchPaths = data->browser->settings->SearchPaths;
			for (auto& sPath : searchPaths) {
				auto pathObj = Util::PathFromString(sPath.path);

				auto handleItem = [browser = data->browser](auto p) {
					auto extension = p.path().extension().u8string();
					auto pathString = p.path().u8string();
					auto filename = p.path().filename().u8string();

					auto it = std::find_if(BrowserExtensions.begin(), BrowserExtensions.end(),
						[&](auto& ext) {
							return std::strcmp(ext.first, extension.c_str()) == 0;
						});
					if (it != BrowserExtensions.end()) {
						// extension matches
						auto funscript = p.path();
						funscript.replace_extension(".funscript");
						auto funscript_path = funscript.u8string();
						bool matchingScript = Util::FileExists(funscript_path);
						if (!matchingScript) return;


						// valid extension + matching script
						size_t byte_count = p.file_size();
						auto timestamp = GetFileAge(p.path());

						Video vid;
						bool updateVid = false;
						{
							auto optVid = Videolibrary::GetVideoByPath(pathString);
							if (optVid.has_value())
							{
								vid = std::move(optVid.value());
								updateVid = true;
								LOGF_INFO("Updating %s", filename.c_str());
							}
						}
						vid.videoPath = pathString;
						vid.byte_count = byte_count;
						vid.videoFilename = filename;
						vid.hasScript = matchingScript;
						vid.timestamp = timestamp;
						vid.shouldGenerateThumbnail = it->second;
						vid.scriptPath = funscript_path;
						
						if (updateVid) { vid.update(); }
						else { 
							if (!vid.try_insert()) { 
								FUN_ASSERT(false, "what!?")
								return; 
							} 
						}

						{
							std::vector<std::string> scriptTags;
							{
								Funscript::Metadata meta;
								meta.loadFromFunscript(funscript_path);
								scriptTags = std::move(meta.tags);
							}
							auto videoTags = Videolibrary::GetTagsForVideo(vid.id);
							if (videoTags.size() > 0 || scriptTags.size() > 0) 
							{
								LOGF_INFO("Updating script tags for %s", vid.videoFilename.c_str());
								std::vector<std::string> dbTagStrings;
								dbTagStrings.reserve(videoTags.size());
								for (auto& dbTag : videoTags) { Tag::NormalizeTagString(dbTag.tag); dbTagStrings.emplace_back(std::move(dbTag.tag)); } videoTags.clear();
								for (auto& sTag : scriptTags) { Tag::NormalizeTagString(sTag); }

								std::sort(dbTagStrings.begin(), dbTagStrings.end());
								std::sort(scriptTags.begin(), scriptTags.end());

								// remove tags from video
								{
									std::vector<std::string> removeTags;
									std::set_difference(
										dbTagStrings.begin(), dbTagStrings.end(),
										scriptTags.begin(), scriptTags.end(),
										std::back_inserter(removeTags)
									);

							
									for (auto& removeTag : removeTags) {
										auto tag = Videolibrary::GetTagByName(removeTag);

										if (tag.has_value()) {
											Videolibrary::Remove<VideoAndTag>(vid.id, tag->id);

											if (Videolibrary::GetTagUsage(tag->id) == 0) {
												// remove
												Videolibrary::Remove<Tag>(tag->id);
											}
										}
									}
								}

								// add tags to video
								{
									std::vector<std::string> addTags;
									std::set_difference(
										scriptTags.begin(), scriptTags.end(),
										dbTagStrings.begin(), dbTagStrings.end(),
										std::back_inserter(addTags)
									);

									for (auto& addTag : addTags) {
										Tag imported;
										imported.tag = addTag;
										imported.try_insert();

										if (imported.id < 0) {
											auto tag = Videolibrary::TagByName(imported.tag);
											FUN_ASSERT(tag.has_value(), "where did the tag go!?");
											imported.id = tag->id;
										}
										VideoAndTag connect;
										connect.tagId = imported.id;
										connect.videoId = vid.id;
										// must be replaced because tagId & videoId are primary keys
										connect.replace();
									}
								}
							}
						}

						if (updateVid) { return; }

						SDL_AtomicLock(&browser->ItemsLock);
						browser->Items.emplace_back(std::move(vid));
						SDL_AtomicUnlock(&browser->ItemsLock);
						LOGF_INFO("found %s", filename.c_str());
					}
				};

				std::error_code ec;
				std::vector<std::future<void>> futures;
				if (sPath.recursive) {
					for (auto& p : std::filesystem::recursive_directory_iterator(pathObj, ec)) {
						if (p.is_directory()) continue;
						futures.emplace_back(std::async(std::launch::async, handleItem, p));
						if (futures.size() == MaxFileSearchFutures) {
							for (auto& future : futures) {
								future.wait();
							}
							futures.clear();
						}
					}
				}
				else {
					for (auto& p : std::filesystem::directory_iterator(pathObj, ec)) {
						if (p.is_directory()) continue;
						futures.emplace_back(std::async(std::launch::async, handleItem, p));
						if (futures.size() == MaxFileSearchFutures) {
							for (auto& future : futures) {
								future.wait();
							}
							futures.clear();
						}
					}
				}
				for (auto& fut : futures) { fut.wait(); }
				LOGF_DEBUG("Done iterating \"%s\" %s", sPath.path.c_str(), sPath.recursive ? "recursively" : "");
			}
		};

		checkForRemovedFiles();
		searchPathsForVideos(data);

		// set items equal to all videos
		{
			auto handle = EventSystem::WaitableSingleShot(
				[](void* ctx)
				{
					auto browser = (Videobrowser*)ctx;
					// this needs to run on the gl thread 
					SDL_AtomicLock(&browser->ItemsLock);
					auto allVideos = Videolibrary::GetAll<Video>();
					browser->setVideos(allVideos);
					SDL_AtomicUnlock(&browser->ItemsLock);
				}
				, &browser);
			handle->wait();
		}

		data->browser->CacheUpdateInProgress = false;
		delete data;
		return 0;
	};

	auto data = new UpdateLibraryThreadData;
	data->browser = this;
	auto thread = SDL_CreateThread(updateLibrary, "UpdateVideoLibrary", data);
	SDL_DetachThread(thread);
}

Videobrowser::Videobrowser(VideobrowserSettings* settings)
	: settings(settings)
{
	Videolibrary::init();
#ifndef NDEBUG
	//foo
	if constexpr(false)
	{
		Videolibrary::DeleteAll();
	}
#endif
	auto vidCache = Videolibrary::GetAll<Video>();
	CacheNeedsUpdate = vidCache.empty();

	preview.setup();

	SDL_AtomicLock(&ItemsLock);
	for (auto& cached : vidCache) {
		Items.emplace_back(std::move(cached));
	}
	std::sort(Items.begin(), Items.end(),
		[](auto& item1, auto& item2) {
			return item1.video.timestamp > item2.video.timestamp;
	});
	SDL_AtomicUnlock(&ItemsLock);	
}

void Videobrowser::Randomizer(const char* Id, bool* open) noexcept
{
	if (open != nullptr && !*open) return;

	ImGui::Begin(Id, open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
	ImGui::BeginChild("RandomVideo");
	renderRandomizer();
	ImGui::EndChild();
	ImGui::End();
}

void Videobrowser::renderRandomizer() noexcept
{
	if (Items.empty()) return;

	constexpr int32_t MaxRollFreqMs = 120;
	constexpr int32_t PickAfterRolls = 69;
	constexpr int32_t ShowResultTime = 3000;

	static int32_t RollFreqMs = MaxRollFreqMs;
	static int32_t RollCount = 0;
	static int32_t LastRollTime = 0;
	static int32_t LastRoll = 0;

	auto& pickedItem = Filter.empty() ? Items[LastRoll] : FilteredItems[LastRoll];
	if (SDL_GetTicks() - LastRollTime >= RollFreqMs)
	{
		if (RollCount == PickAfterRolls) {
			fileClickedHandler(pickedItem);
			RollFreqMs = MaxRollFreqMs;
			RollCount = 0;
			Random = false;
		}
		else
		{
			if (RollCount == 0)
			{
				srand(SDL_GetTicks());
			}
			LastRoll = rand() % (Filter.empty() ? Items.size() : FilteredItems.size());
			LastRollTime = SDL_GetTicks();
			RollCount++;

			if (RollCount != PickAfterRolls)
			{
				// roll faster which each roll
				auto easeOutCubic = [](float x) -> float {	x = 1 - x;	return 1 - (x*x*x);	};
				RollFreqMs = MaxRollFreqMs - ((MaxRollFreqMs / 1.25f) * (easeOutCubic((float)RollCount/PickAfterRolls)));
			}
			else
			{
				// show result for 3 seconds
				RollFreqMs = ShowResultTime;
			}
		}
	}

	auto& style = ImGui::GetStyle();
	auto window_pos = ImGui::GetWindowPos();
	auto availSize = ImGui::GetContentRegionAvail() - style.ItemSpacing;
	auto window = ImGui::GetCurrentWindowRead();
	auto frame_bb = ImRect(window->DC.CursorPos, window->DC.CursorPos + availSize);
	auto draw_list = ImGui::GetWindowDrawList();
	
	constexpr float scale = 3.f;
	//ImGui::ItemSize(availSize, 0.0f);
	//if (!ImGui::ItemAdd(frame_bb, 0))
	//	return;

	ImVec2 videoSize((availSize.x / scale) * (16.f / 9.f), (availSize.x / scale));

	ImVec2 centerPos(frame_bb.GetWidth() / 2.f, frame_bb.GetHeight() / 2.f);
	centerPos += frame_bb.Min;
	
	ImVec2 thumbPos = centerPos;
	ImVec2 p1 = thumbPos - (videoSize / 2.f);
	ImVec2 p2 = thumbPos + (videoSize / 2.f);
	draw_list->AddRect(p1, p2, ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Border]), 2.f);

	uint32_t texId = pickedItem.texture.GetTexId();
	
	if(RollCount == PickAfterRolls)
	{
		if (!preview.loading)
		{
			preview.previewVideo(pickedItem.video.videoPath, 0.1f);
		}
		if (preview.ready)
		{
			texId = preview.render_texture;
		}
	}

	if (texId != 0) {
		draw_list->AddImage((
			void*)(intptr_t)texId,
			p1 + style.FramePadding, p2 - style.FramePadding,
			ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE);
	}
	else
	{
		pickedItem.GenThumbail();
	}
	auto textSize = ImGui::CalcTextSize(pickedItem.video.videoFilename.c_str());
	draw_list->AddText(
		p1 + ImVec2((videoSize.x/2.f) - (textSize.x/2.f), videoSize.y + style.ItemSpacing.y),
		ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]), pickedItem.video.videoFilename.c_str());

	if (RollCount == PickAfterRolls)
	{
		stbsp_snprintf(tmp, sizeof(tmp), "%.2f seconds", (ShowResultTime - (SDL_GetTicks() - LastRollTime)) / 1000.f);
		auto countDownTextSize = ImGui::CalcTextSize(tmp);
		draw_list->AddText(
			p1 + ImVec2((videoSize.x / 2.f) - (countDownTextSize.x / 2.f), textSize.y + videoSize.y + (style.ItemSpacing.y*2.f)),
			ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
			tmp
		);
	}
	else
	{
		draw_list->AddRectFilledMultiColor(
			p1 + ImVec2(0.f, videoSize.y + textSize.y + style.ItemSpacing.y),
			p1 + ImVec2(videoSize.x * (RollCount / (float)PickAfterRolls), ImGui::GetFontSize() + videoSize.y + textSize.y + style.ItemSpacing.y),
			IM_COL32_BLACK, 
			IM_COL32(255, 0, 0, 255), 
			IM_COL32(255, 0, 0, 255),
			IM_COL32_BLACK
		);
	}
}

void Videobrowser::fileClickedHandler(VideobrowserItem& item) noexcept
{
	if (item.video.hasScript) {
		ClickedFilePath = item.video.videoPath;
		EventSystem::PushEvent(VideobrowserEvents::VideobrowserItemClicked);
	}
}

void Videobrowser::ShowBrowser(const char* Id, bool* open) noexcept
{
	uint32_t window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoScrollbar;

	if (open != NULL && !*open) return;
	if (CacheUpdateInProgress) {
		ImGui::Begin(Id, open, window_flags);
		ImGui::TextUnformatted("Updating library... this may take a while.");
		ImGui::NewLine();
		ImGui::Text("Found %ld new videos", Items.size());
		ImGui::SameLine(); OFS::Spinner("it do be spinning doe", ImGui::GetFontSize()/2.f, ImGui::GetFontSize()/4.f, IM_COL32(66, 150, 250, 255));
		ImGui::End();
		return;
	}
	if (CacheNeedsUpdate) { updateLibraryCache(); }

	if (Random)
	{
		SDL_AtomicLock(&ItemsLock);
		Randomizer(Id, open);
		SDL_AtomicUnlock(&ItemsLock);
		return;
	}

	// gamepad control items per row
	if (ImGui::IsNavInputTest(ImGuiNavInput_FocusPrev, ImGuiInputReadMode_Pressed))
	{
		settings->ItemsPerRow--;
		settings->ItemsPerRow = Util::Clamp(settings->ItemsPerRow, 1, 25);
	}
	else if (ImGui::IsNavInputTest(ImGuiNavInput_FocusNext, ImGuiInputReadMode_Pressed)) 
	{
		settings->ItemsPerRow++;
		settings->ItemsPerRow = Util::Clamp(settings->ItemsPerRow, 1, 25);
	}
	
	
	window_flags |= ImGuiWindowFlags_MenuBar;
	SDL_AtomicLock(&ItemsLock);
	ImGui::SetNextWindowScroll(ImVec2(0,0)); // prevent scrolling

	ImGui::Begin(Id, open, window_flags);
	auto availSpace = ImGui::GetContentRegionMax();
	auto& style = ImGui::GetStyle();
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("View")) {
			ImGui::MenuItem("Show thumbnails", NULL, &settings->showThumbnails);
			ImGui::SetNextItemWidth(ImGui::GetFontSize()*5.f);
			ImGui::InputInt("Items", &settings->ItemsPerRow, 1, 10);
			settings->ItemsPerRow = Util::Clamp(settings->ItemsPerRow, 1, 25);
			ImGui::EndMenu();
		}
		ImGui::MenuItem("Settings", NULL, &ShowSettings);
		ImGui::MenuItem("Random", NULL, &Random, Filter.empty() ? Items.size() > 0 : FilteredItems.size() > 0);
		ImGui::EndMenuBar();
	}

	if (ImGui::Button(ICON_REFRESH)) {
		CacheNeedsUpdate = true;
	}
	ImGui::SameLine();
	ImGui::Bullet();
	ImGui::Text("Videos: %ld", Filter.empty() ? Items.size() : FilteredItems.size());
	ImGui::Separator();
	
		
	if (ImGui::BeginTable("##VideobrowserUI", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings)) {
		ImGui::TableSetupColumn("##TagCol", ImGuiTableColumnFlags_WidthStretch, 0.15);
		ImGui::TableSetupColumn("##ItemsCol", ImGuiTableColumnFlags_WidthStretch, 0.85);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();


		ImGui::BeginChild("Tags");
		if (Tags.size() != Videolibrary::Count<Tag>()) {
			Tags = Videolibrary::GetAll<Tag>();
		}

		if (ImGui::Button("All##AllTagsButton", ImVec2(-1, 0))) {
			for (auto& t : Tags) { t.FilterActive = false; }
		}
		static std::vector<int64_t> activeFilterTags;
		int activeCount = activeFilterTags.size();
		activeFilterTags.clear();

		{
			int deleteIndex = -1;
			for (int i = 0; i < Tags.size(); i++) {
				ImGui::PushID(i);
				auto& tagItem = Tags[i];
				int32_t usageCount = tagItem.UsageCount();
				stbsp_snprintf(tmp, sizeof(tmp), "%s (%d)", tagItem.tag.c_str(), usageCount);
				ImGui::Checkbox(tmp, &tagItem.FilterActive);
				if (usageCount == 0) {
					ImGui::SameLine();
					if(ImGui::Button(ICON_TRASH)) {
						deleteIndex = i;
					}
				}
				if (tagItem.FilterActive) { activeFilterTags.emplace_back(tagItem.id); }
				ImGui::PopID();
			}
			if (deleteIndex >= 0) { Videolibrary::Remove<Tag>(Tags[deleteIndex].id);  }
		}

		if (activeCount != activeFilterTags.size()) {
			if (activeFilterTags.size() > 0) {
				auto vids = Videolibrary::GetVideosWithTags(activeFilterTags);
				setVideos(vids);
			}
			else
			{
				// all
				auto vids = Videolibrary::GetAll<Video>();
				setVideos(vids);
			}
		}
		ImGui::Separator();
		auto addTag = [](const std::string& newTag) {
			Tag tag;
			tag.tag = newTag;
			tag.NormalizeTagString(tag.tag);
			tag.insert();
		};
		if (ImGui::InputText("##TagInput", &NewTagBuffer, ImGuiInputTextFlags_EnterReturnsTrue)) { addTag(NewTagBuffer); }
		ImGui::SameLine();
		if (ImGui::Button("Add##AddTag", ImVec2(-1, 0))) { addTag(NewTagBuffer); }
		ImGui::EndChild();

		ImGui::TableNextColumn();

		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputText("Filter", &Filter))
		{
			Util::trim(Filter);
			if (!Filter.empty()) {
				FilteredItems.clear();
				for (auto& item : Items)
				{
					if (Util::ContainsInsensitive(item.video.videoFilename, Filter))
					{
						FilteredItems.emplace_back(item);
					}
				}
			}
		}

		// HACK: there currently doesn't seem to be a way to obtain the column width
		const auto& column = ImGui::GetCurrentContext()->CurrentTable->Columns[ImGui::TableGetColumnIndex()];
		float ItemWidth = ((availSpace.x * column.StretchWeight)
			- style.ScrollbarSize 
			- (3.f * style.ItemInnerSpacing.x)
			- (settings->ItemsPerRow*style.ItemSpacing.x)) / (float)settings->ItemsPerRow;
		ItemWidth = std::max(ItemWidth, 3.f);
		const float ItemHeight = (9.f/16.f)*ItemWidth;
		const auto ItemDim = ImVec2(ItemWidth, ItemHeight);

		ImGui::BeginChild("Items");
		VideobrowserItem* previewItem = nullptr;
		bool itemFocussed = false;
		int index = 0;
		for (auto& item : Filter.empty() ? Items : FilteredItems) {
			ImColor FileTintColor = item.video.hasScript ? IM_COL32_WHITE : IM_COL32(200, 200, 200, 255);
			if (!item.Focussed) {
				FileTintColor.Value.x *= 0.75f;
				FileTintColor.Value.y *= 0.75f;
				FileTintColor.Value.z *= 0.75f;
			}

			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_PlotLinesHovered]);
			ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_PlotLines]);

			ImGui::PushID(index);
			auto texId = item.texture.GetTexId();
			if (settings->showThumbnails && texId != 0) {
				ImVec2 padding = item.video.hasScript ? ImVec2(0, 0) : ImVec2(ItemWidth*0.1f, ItemWidth*0.1f);
				if(ImGui::ImageButtonEx(ImGui::GetID(item.video.videoPath.c_str()),
					item.Focussed && preview.ready ? (void*)(intptr_t)preview.render_texture : (void*)(intptr_t)texId,
					ItemDim - padding,
					ImVec2(0, 0), ImVec2(1, 1),
					padding/2.f,
					style.Colors[ImGuiCol_PlotLines],
					FileTintColor)) {
					fileClickedHandler(item);
				}
				item.Focussed = (ImGui::IsItemHovered() || (ImGui::IsItemActive() || ImGui::IsItemActivated() || ImGui::IsItemFocused())) && !itemFocussed;
				if (item.Focussed) {
					itemFocussed = true;
					previewItem = &item;
				}
			}
			else {
				if(!item.video.hasScript) { ImGui::PushStyleColor(ImGuiCol_Button, FileTintColor.Value); }
				
				if (ImGui::Button(item.video.videoFilename.c_str(), ItemDim)) {
					fileClickedHandler(item);
				}
				if (settings->showThumbnails && item.video.HasThumbnail() && ImGui::IsItemVisible()) {
					item.GenThumbail();
				}
				if (!item.video.hasScript) { ImGui::PopStyleColor(1); }
			}
			ImGui::PopID();
			ImGui::PopStyleColor(2);
		
			if (ImGui::BeginPopupContextItem() || OFS::GamepadContextMenu())
			{
				if (ImGui::BeginMenu("Add tag...")) {
					for (auto& t : Tags) {
						if (ImGui::MenuItem(t.tag.c_str())) {
							VideoAndTag connect;
							connect.tagId = t.id;
							connect.videoId = item.video.id;
							connect.replace();
							Videolibrary::WritebackFunscriptTags(item.video.id, item.video.scriptPath);
						}
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Remove tag...")) {
					item.UpdateTags();
					for (auto& t : item.AssignedTags) {
						if (ImGui::MenuItem(t.tag.c_str())) {
							Videolibrary::Remove<VideoAndTag>(item.video.id, t.id);
							Videolibrary::WritebackFunscriptTags(item.video.id, item.video.scriptPath);
						}
					}
					if (item.AssignedTags.empty()) ImGui::TextDisabled("Nothing here.");
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Explore location...")) {
					auto path = Util::PathFromString(item.video.videoPath);
					path.remove_filename();
					Util::OpenFileExplorer(path.u8string().c_str());
				}
				ImGui::EndPopup();
			}
			else {
				Util::Tooltip(item.video.videoFilename.c_str());
			}

			index++;
			if (index % settings->ItemsPerRow != 0) {
				ImGui::SameLine();
			}
		}

		if (previewItem != nullptr && !preview.loading || previewItem != nullptr && previewItem->texture.Id != previewItemId) {
			previewItemId = previewItem->texture.Id;
			preview.previewVideo(previewItem->video.videoPath, 0.2f);
		}
		else if (previewItem == nullptr && preview.ready) {
			preview.closeVideo();
		}

		ImGui::EndChild();

		ImGui::EndTable();
	}
	
	ImGui::End();
	ShowBrowserSettings(&ShowSettings);
	SDL_AtomicUnlock(&ItemsLock);
}

void Videobrowser::ShowBrowserSettings(bool* open) noexcept
{
	if (open != nullptr && !*open) return;
	
	if (ShowSettings)
		ImGui::OpenPopup(VideobrowserSettingsId);

	if (ImGui::BeginPopupModal(VideobrowserSettingsId, open, ImGuiWindowFlags_None)) {
		if (ImGui::BeginTable("##SearchPaths", 3, ImGuiTableFlags_Borders)) {
			ImGui::TableSetupColumn("Path");
			//ImGui::TableSetupColumn("Recursive");
			//ImGui::TableSetupColumn("");
			ImGui::TableHeadersRow();
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
		ImGui::NewLine(); ImGui::Separator();
		if (ImGui::Button("Clear database", ImVec2(-1, 0))) {
			Videolibrary::DeleteAll();
			auto allVideos = Videolibrary::GetAll<Video>();
			setVideos(allVideos); // items are locked by the caller
		}

		Util::ForceMinumumWindowSize(ImGui::GetCurrentWindow());
		ImGui::EndPopup();
	}
}

int32_t VideobrowserEvents::VideobrowserItemClicked = 0;

void VideobrowserEvents::RegisterEvents() noexcept
{
	VideobrowserItemClicked = SDL_RegisterEvents(1);
}
