#include "OFS_Profiling.h"

#include "imgui.h"

std::vector<OFS_Codepath> OFS_Profiler::Stack;
std::vector<OFS_Codepath> OFS_Profiler::Frame;
std::vector<OFS_Codepath> OFS_Profiler::LastFrame;

static void Print(const std::vector<OFS_Codepath>& paths) noexcept
{
	for (int i = paths.size()-1; i >= 0; i--) {
		auto& p = paths[i];
		float startX = ImGui::GetStyle().ItemInnerSpacing.x + (p.Depth * ImGui::GetFontSize());
		ImGui::SetCursorPosX(startX);
		ImGui::Text("|- %s: %.3f ms", p.Name.c_str(), p.Duration.count());

		if (!p.Children.empty()) {
			Print(p.Children);
		}
	}
}

void OFS_Profiler::ShowProfiler() noexcept
{
	ImGui::Begin("OFS Profiler");
	if (!LastFrame.empty()) {
		Print(LastFrame);
	}
	ImGui::End();
}

void OFS_Profiler::BeginProfiling() noexcept
{
	Stack.clear();
	Frame.clear();
}

void OFS_Profiler::EndProfiling() noexcept
{
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
