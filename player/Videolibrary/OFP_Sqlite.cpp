#include "OFP_Sqlite.h"

#include "Funscript.h"

//StorageT Videolibrary::Storage = initStorage(Util::PrefpathOFP("library.sqlite"));

SDL_atomic_t Videolibrary::Reads = { 0 };
SDL_atomic_t Videolibrary::QueuedWrites = { 0 };
SDL_sem* Videolibrary::WriteSem = SDL_CreateSemaphore(1);

std::optional<Thumbnail> Video::thumbnail() const
{
	return std::move(Videolibrary::Get<Thumbnail>(thumbnailId));
}

bool Videolibrary::WritebackFunscriptTags(int64_t videoId, const std::string& funscriptPath) noexcept
{
	auto tags = Videolibrary::GetTagsForVideo(videoId);
	Funscript::Metadata meta;
	if (meta.loadFromFunscript(funscriptPath))
	{
		meta.tags.clear();
		meta.tags.reserve(tags.size());
		for (auto& tag : tags) { meta.tags.emplace_back(std::move(tag.tag)); }
		return meta.writeToFunscript(funscriptPath);
	}
	return false;
}

int32_t Tag::UsageCount()
{
	return Videolibrary::GetTagUsage(id);
}
