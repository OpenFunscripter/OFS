#include "OFP_Sqlite.h"


StorageT Videolibrary::Storage = initStorage(Util::PrefpathOFP("library.sqlite"));

template<typename T>
static std::unique_ptr<T> ById(const std::shared_ptr<int64_t>& id) {
	try {
		auto result =  id != nullptr ? std::move(Videolibrary::Storage.get_pointer<T>(*id)) : nullptr;
		return result;
	}
	catch (std::system_error& er) {
		LOGF_ERROR("%s", er.what());
	}
	return nullptr;
}

template<typename T>
static std::unique_ptr<T> ById(int32_t id) noexcept {
	return std::move(Videolibrary::Storage.get_pointer<T>(id));
}

std::unique_ptr<Thumbnail> Video::thumbnail() const
{
	return std::move(ById<Thumbnail>(thumbnailId));
}
