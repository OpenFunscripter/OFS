#include "OFS_Profiling.h"

#include "imgui.h"

std::map<std::string, OFS_Codepath> OFS_Profiler::Paths;


void OFS_Profiler::ShowProfiler() noexcept
{
	ImGui::Begin("OFS Profiler");
	for (auto& path : Paths) {
		ImGui::Text("%35s: %.3f ms", path.first.c_str(), path.second.LastDuration.count());
	}
	ImGui::End();
}