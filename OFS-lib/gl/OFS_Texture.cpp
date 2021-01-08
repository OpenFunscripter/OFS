#include "OFS_Texture.h"

//std::unordered_map<uint64_t, OFS_Texture::Texture> OFS_Texture::TextureHashtable;
std::vector<OFS_Texture::Texture> OFS_Texture::Textures;

int32_t  OFS_Texture::LastFreeIndex = 0;

SDL_atomic_t OFS_Texture::Reads = { 0 };
SDL_atomic_t OFS_Texture::QueuedWrites = { 0 };
SDL_atomic_t OFS_Texture::TextureIdCounter = { 1 };
SDL_sem* OFS_Texture::WriteSem = SDL_CreateSemaphore(1);