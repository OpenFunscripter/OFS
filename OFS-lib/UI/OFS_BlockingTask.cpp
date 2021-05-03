#include "OFS_BlockingTask.h"
#include "OFS_ImGui.h"
#include "imgui.h"

static int BlockingTaskThread(void* data) noexcept
{
	auto bdata = (OFS_BlockingTask*)data;
	bdata->currentTask->TaskThreadFunc(bdata->currentTask.get());
	bdata->currentTask.reset();
	bdata->Running = false;
	return 0;
}

void OFS_BlockingTask::ShowBlockingTask() noexcept
{
	constexpr const char* ID = "Running task##BlockingTaskModal";
	if (currentTask) {
		ImGui::OpenPopup(ID, ImGuiPopupFlags_None);
	}
	else { return; }

	if (!Running) {
		Running = true;
		auto thread = SDL_CreateThread(BlockingTaskThread, "BlockingTaskThread", this);
		SDL_DetachThread(thread);
	}

	auto& style = ImGui::GetStyle();
	ImGui::BeginPopupModal(ID, NULL, ImGuiWindowFlags_AlwaysAutoResize);
	const bool ShowProgress = currentTask->MaxProgress > 0;
	if (ShowProgress) {
		ImGui::Text("This may take a while... (%d/%d)", currentTask->Progress, currentTask->MaxProgress);
	}
	else {
		ImGui::Text("This may take a while...");
	}
	ImGui::SameLine();
	OFS::Spinner("BlockingTaskSpinner", ImGui::GetFontSize()/2.f, ImGui::GetFontSize() / 4.f, ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_ButtonActive]));
	if (ShowProgress) {
		ImGui::ProgressBar(currentTask->Progress / (float)currentTask->MaxProgress,
			ImVec2(-1.f, 0.f),
			currentTask->TaskDescription); 
		ImGui::SameLine();
	}
	ImGui::EndPopup();
}