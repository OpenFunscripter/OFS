#pragma

#include "OFS_Util.h"

#include <vector>
#include <cstdint>

#include "glad/glad.h"

#include "SDL_atomic.h"
#include "SDL_mutex.h"

class OFS_Texture {
private:
	class Texture {
	public:
		int32_t ref_count = 0;
		uint32_t texId = 0;

		Texture() noexcept
		{
			LOG_DEBUG("Created texture.");
		}

		~Texture() noexcept {
			if (ref_count == 0) {
				freeTexture();
			}
		}

		inline void freeTexture() noexcept {
			if (texId != 0) {
				FUN_ASSERT(ref_count == 0, "ref_count not zero");
				glDeleteTextures(1, &texId);
				texId = 0;
				ref_count = 0;
				LOG_DEBUG("Freed texture.");
			}
		}
	};
	static std::vector<Texture> Textures;

	static SDL_atomic_t TextureIdCounter;
	static SDL_atomic_t Reads;
	static SDL_atomic_t QueuedWrites;
	static SDL_sem* WriteSem;

	inline static bool BoundCheck(uint64_t id) noexcept {
		id--; // ids start at one
		return id >= 0 && id < Textures.size();
	}
	inline static Texture& ById(uint64_t id) noexcept {
		return Textures[id-1];
	}
public:
	inline static void ReadTextures() noexcept {
		for (;;) {
			int value = SDL_AtomicGet(&Reads);
			int queuedWrites = SDL_AtomicGet(&QueuedWrites);
			if (value > 0 || queuedWrites > 0) { continue; }
			if (SDL_AtomicCAS(&Reads, value, value - 1)) {
				//LOGF_DEBUG("READ: Writes:%d", value - 1);	
				break; 
			}
		}

		// after reading SDL_AtomicIncRef(&Reads);
	}

	inline static void WriteTextures() noexcept {
		SDL_AtomicIncRef(&QueuedWrites);
		int queuedWrites;
		for (;;) {
			queuedWrites = SDL_AtomicGet(&QueuedWrites);
			if (SDL_AtomicCAS(&QueuedWrites, queuedWrites, 0)) { break; }
		}

		for (;;) {
			int value = SDL_AtomicGet(&Reads);
			if (value < 0) { continue; }
			if (value > 0 && queuedWrites == 0) { break; }
			if (SDL_AtomicCAS(&Reads, 0, queuedWrites)) {
				//LOGF_DEBUG("WRITE: Writes: %d", queuedWrites);
				break; 
			}
		}
		SDL_SemWait(WriteSem);
		// after writing SDL_AtomicDecRef(&Reads) + SDL_SemPost(WriteSem)
	}
	inline static void EndWriteTextures() noexcept 
	{
		SDL_AtomicDecRef(&Reads);
		SDL_SemPost(WriteSem);
	}
	
	class Handle {
	private:
		inline void IncrementRefCount() noexcept {
			ReadTextures(); // it's a "read" because it doesn't erase or append
			if (BoundCheck(this->Id)) {
				ById(this->Id).ref_count++;
			}
			SDL_AtomicIncRef(&Reads);
		}

		inline void DecrementRefCount() noexcept {
			ReadTextures(); // it's a "read" because it doesn't erase or append
			if (BoundCheck(this->Id)) {
				ById(this->Id).ref_count--;
				if (ById(this->Id).ref_count <= 0) {
					// this needs to run on the main thread...
					ById(this->Id).freeTexture();
				}
			}
			SDL_AtomicIncRef(&Reads);
		}
	public:
		uint64_t Id = 0;
		Handle() noexcept = default;

		Handle(uint64_t id) : Id(id) {
			IncrementRefCount();
		}
		~Handle() noexcept {
			DecrementRefCount();
		}

		Handle(const Handle& h) noexcept {
			if(this->Id != 0) {
				DecrementRefCount();
			}
			this->Id = h.Id;
			IncrementRefCount();
		}

		Handle(Handle&& h) noexcept {
			if(this->Id != 0) {
				DecrementRefCount();
			}
			this->Id = h.Id;
			IncrementRefCount();
		}

		Handle& operator=(const Handle& h) noexcept {
			if(this->Id != 0) {
				DecrementRefCount();
			}
			this->Id = h.Id;
			IncrementRefCount();
			return *this;
		}

		Handle& operator=(Handle&& h) noexcept {
			if(this->Id != 0) {
				DecrementRefCount();
			}
			this->Id = h.Id;
			IncrementRefCount();
			return *this;
		}

		inline uint32_t GetTexId() noexcept {
			ReadTextures();
			if (BoundCheck(this->Id)) {
				SDL_AtomicIncRef(&Reads);
				return ById(this->Id).texId;
			}
			SDL_AtomicIncRef(&Reads);
			return 0;
		}

		inline void SetTexId(uint32_t texId) noexcept 
		{
			// same as read because nothing is inserted or erased
			ReadTextures(); 
			if (BoundCheck(this->Id)) {
				FUN_ASSERT(ById(this->Id).texId == 0, "leaking texture");
				ById(this->Id).texId = texId;
			}
			SDL_AtomicIncRef(&Reads);
		}
	};

	inline static Handle CreateTexture() noexcept {
		WriteTextures();
		int newId = SDL_AtomicIncRef(&TextureIdCounter);
		Textures.emplace_back();
		EndWriteTextures();
		Handle handle(newId);
		return handle;
	}

};

