#include "OFS_Profiling.h"

#include "imgui.h"

std::vector<OFS_Codepath> OFS_Profiler::Stack;
std::vector<OFS_Codepath> OFS_Profiler::Frame;
std::vector<OFS_Codepath> OFS_Profiler::LastFrame;

bool OFS_Profiler::Live = true;
bool OFS_Profiler::RecordOnce = false;
uint32_t OFS_Profiler::EventCount = 0;
uint32_t OFS_Profiler::MaxEventsAllTime = 0;

static bool LiveTmp = true;
static bool RecordOnceTmp = false;

static void Print(const std::vector<OFS_Codepath>& paths) noexcept
{
	for (int i = paths.size()-1; i >= 0; i--) {
		auto& p = paths[i];
		float startX = ImGui::GetStyle().WindowPadding.x + (p.Depth * ImGui::GetFontSize());
		ImGui::SetCursorPosX(startX);
		ImGui::Text("%s: %.3f ms", p.Name.c_str(), p.Duration.count());

		if (!p.Children.empty()) {
			Print(p.Children);
		}
	}
}

void OFS_Profiler::ShowProfiler() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	ImGui::Begin("OFS Profiler");
	ImGui::Text("Events this Frame: %d\nMax events: %d", EventCount, MaxEventsAllTime);
	ImGui::Separator();
	if (ImGui::Button("Record one frame")) {
		RecordOnceTmp = true;
	}
	ImGui::SameLine();
	ImGui::Checkbox("Live", &LiveTmp);

	if (!LastFrame.empty()) {
		Print(LastFrame);
	}
	ImGui::End();
}

void OFS_Profiler::BeginProfiling() noexcept
{
	RecordOnce = RecordOnceTmp; RecordOnceTmp = false;
	Live = LiveTmp;
	if (!Live && !RecordOnce) return;
	Stack.clear();
	Frame.clear();
}

void OFS_Profiler::EndProfiling() noexcept
{
	if (!Live && !RecordOnce) return;
	RecordOnce = false;
	LastFrame.clear();
	int depth = 0;
	
	std::vector<std::vector<OFS_Codepath>*> stack;
	stack.push_back(&LastFrame);

	for (int i = Frame.size() - 1; i >= 0; i--) {
		auto& p = Frame[i];
		
		if (p.Depth > depth) {
			stack.push_back(&stack.back()->back().Children);
			auto& f = stack.back()->emplace_back(std::move(p));
		}
		else if (p.Depth < depth) {
			do { stack.pop_back(); } while (p.Depth < stack.back()->back().Depth);
			auto& f = stack.back()->emplace_back(std::move(p));
		}
		else {
			auto& f = stack.back()->emplace_back(std::move(p));
		}

		depth = p.Depth;
	}
}
