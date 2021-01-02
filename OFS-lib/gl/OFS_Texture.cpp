#include "OFS_Texture.h"

std::unordered_map<uint64_t, OFS_Texture::Texture> OFS_Texture::TextureHashtable;


SDL_atomic_t OFS_Texture::Reads = { 0 };
SDL_atomic_t OFS_Texture::QueuedWrites = { 0 };
