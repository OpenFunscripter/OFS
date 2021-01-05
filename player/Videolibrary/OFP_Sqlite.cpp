#include "OFP_Sqlite.h"

//StorageT Videolibrary::Storage = initStorage(Util::PrefpathOFP("library.sqlite"));

SDL_atomic_t Videolibrary::Reads = { 0 };
SDL_atomic_t Videolibrary::QueuedWrites = { 0 };
SDL_sem* Videolibrary::WriteSem = SDL_CreateSemaphore(1);

std::optional<Thumbnail> Video::thumbnail() const
{
	return std::move(Videolibrary::Get<Thumbnail>(thumbnailId));
}
