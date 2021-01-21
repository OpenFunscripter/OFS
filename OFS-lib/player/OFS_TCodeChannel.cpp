#include "OFS_TCodeChannel.h"

TCodeEasing TCodeChannel::EasingMode = TCodeEasing::None;


std::array<const std::vector<const char*>, static_cast<size_t>(TChannel::TotalCount)> TCodeChannels::Aliases
{
	std::vector<const char*>{"l0", "L0", "stroke" },
	std::vector<const char*>{"l1", "L1", "sway" },
	std::vector<const char*>{"l2", "L2", "surge" },
	std::vector<const char*>{"r0", "R0", "twist" },
	std::vector<const char*>{"r1", "R1", "roll" },
	std::vector<const char*>{"r2", "R2", "pitch" },
	std::vector<const char*>{"v0", "V0", "vib" },
	std::vector<const char*>{"v1", "V1", "pump" },
	std::vector<const char*>{"v2", "V2", "unknown"}
};