#include "OFP_Settings.h"
#include "OFS_Util.h"
#include "OFS_Serialization.h"

#include "nlohmann/json.hpp"

bool OFP_Settings::load(const std::string& path) noexcept
{
	loadedPath = path;
	bool succ = false;
	auto json = Util::LoadJson(path, &succ);
	if (succ) {
		OFS::serializer::load(this, &json);
	}
	return succ;
}

void OFP_Settings::save() noexcept
{
	nlohmann::json json;
	OFS::serializer::save(this, &json);
	Util::WriteJson(json, loadedPath, true);
}
