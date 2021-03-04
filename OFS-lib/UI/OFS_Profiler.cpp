#include "OFS_Profiling.h"

#include "imgui.h"

std::vector<OFS_Codepath> OFS_Profiler::Stack;
std::vector<OFS_Codepath> OFS_Profiler::Frame;
std::vector<OFS_Codepath> OFS_Profiler::LastFrame;

void OFS_Profiler::ShowProfiler() noexcept
{
	ImGui::Begin("OFS Profiler");

	if (!LastFrame.empty()) {
		auto startX = ImGui::GetCursorPosX();
		int currentDepth = LastFrame.back().Depth;
		for (int i = LastFrame.size() - 1; i != 0; i--) {
			auto& path = LastFrame[i];
			if (path.Depth != currentDepth) startX = path.Depth * ImGui::GetFontSize();

			ImGui::SetCursorPosX(startX);
			ImGui::Text("> %s: %.3f ms", path.Name.c_str(), path.Duration.count());
			currentDepth = path.Depth;
		}
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
	LastFrame.swap(Frame);
}
