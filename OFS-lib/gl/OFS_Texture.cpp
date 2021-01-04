#include "OFS_Texture.h"

//std::unordered_map<uint64_t, OFS_Texture::Texture> OFS_Texture::TextureHashtable;
std::vector<OFS_Texture::Texture> OFS_Texture::Textures;


SDL_atomic_t OFS_Texture::Reads = { 0 };
SDL_atomic_t OFS_Texture::QueuedWrites = { 0 };
SDL_atomic_t OFS_Texture::TextureIdCounter = { 1 };