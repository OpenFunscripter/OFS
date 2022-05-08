#include "OFS_BlockingTask.h"
#include "OFS_ImGui.h"
#include "OFS_Localization.h"
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
	if (currentTask) {
		ImGui::OpenPopup(TR_ID("RUNNING_TASK", Tr::RUNNING_TASK), ImGuiPopupFlags_None);
	}
	else { return; }

	if (!Running) {
		RunningTimer = 0.f;
		Running = true;
		auto thread = SDL_CreateThread(BlockingTaskThread, "BlockingTaskThread", this);
		SDL_DetachThread(thread);
	}
	RunningTimer += ImGui::GetIO().DeltaTime;

	auto& style = ImGui::GetStyle();
	float a = RunningTimer / 1.f;
	if (!currentTask->DimBackground) {
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, Util::Clamp(a*a, 0.f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 0));
	}
	ImGui::BeginPopupModal(TR_ID("RUNNING_TASK", Tr::RUNNING_TASK), NULL, ImGuiWindowFlags_AlwaysAutoResize);
	if (a >= 0.1f) {
		const bool ShowProgress = currentTask->MaxProgress > 0;
		if (ShowProgress) {
			ImGui::Text("%s (%d/%d)", TR(THIS_MAY_TAKE_A_WHILE), currentTask->Progress, currentTask->MaxProgress);
		}
		else {
			ImGui::TextUnformatted(TR(THIS_MAY_TAKE_A_WHILE));
		}
		ImGui::SameLine();
		OFS::Spinner("BlockingTaskSpinner", ImGui::GetFontSize()/2.f, ImGui::GetFontSize() / 4.f, ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_ButtonActive]));
		if (ShowProgress) {
			ImGui::ProgressBar(currentTask->Progress / (float)currentTask->MaxProgress,
				ImVec2(-1.f, 0.f),
				currentTask->TaskDescription); 
			ImGui::SameLine();
		}
	}
	ImGui::EndPopup();
	if (!currentTask->DimBackground) {
		ImGui::PopStyleVar(1);
		ImGui::PopStyleColor(1);
	}
}