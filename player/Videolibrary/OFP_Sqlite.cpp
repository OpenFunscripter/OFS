#include "OFP_Sqlite.h"

template<typename T>
static std::unique_ptr<T> ById(const std::unique_ptr<int32_t>& id) noexcept {
	return id != nullptr ? std::move(Videolibrary::Storage().get_pointer<T>(*id)) : nullptr;
}

template<typename T>
static std::unique_ptr<T> ById(int32_t id) noexcept {
	return std::move(Videolibrary::Storage().get_pointer<T>(id));
}



std::unique_ptr<Thumbnail> Video::thumbnail() const
{
	return std::move(ById<Thumbnail>(thumbnailId));
}
