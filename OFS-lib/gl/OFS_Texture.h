#pragma

#include "OFS_Util.h"

#include <unordered_map>
#include <cstdint>

#include "glad/glad.h"

#include "SDL_atomic.h"
#include "SDL_mutex.h"
#include "SDL_timer.h"

class OFS_Texture {
private:
	class Texture {
	public:
		int32_t ref_count = 0;
		uint32_t texId = 0;

		~Texture() noexcept {
			if (texId != 0) {
				FUN_ASSERT(ref_count == 0, "ref_count not zero");
				glDeleteTextures(1, &texId);
				texId = 0;
				LOG_DEBUG("Freed texture.");
			}
		}
	};
	static std::unordered_map<uint64_t, Texture> TextureHashtable;
	static SDL_mutex* texLock;
	static SDL_cond* texCond;
	static SDL_sem* TextureReads;

	static SDL_atomic_t Reads;
	static SDL_atomic_t QueuedWrites;
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

		// after reading SDL_AtomicIncRef(&Writes);
	}

	inline static void WriteTextures() noexcept {
		
		int queuedWrites;
		for (;;) {
			queuedWrites = SDL_AtomicGet(&QueuedWrites);
			if (SDL_AtomicCAS(&QueuedWrites, queuedWrites, 0)) { break; }
		}

		for (;;) {
			int value = SDL_AtomicGet(&Reads);
			if (value < 0) { continue; }
			if (value > 0) { break; }
			if (SDL_AtomicCAS(&Reads, 0, queuedWrites)) {
				//LOGF_DEBUG("WRITE: Writes: %d", queuedWrites);
				break; 
			}
		}
		// after writing SDL_AtomicDecRef(&Writes);
	}
	
	class Handle {
	private:
		inline void IncrementRefCount() noexcept {
			ReadTextures();
			auto it = TextureHashtable.find(this->Id);
			if (it != TextureHashtable.end()) {
				it->second.ref_count++;
			}
			SDL_AtomicIncRef(&Reads);
		}

		inline void DecrementRefCount() noexcept {
			ReadTextures();
			auto it = TextureHashtable.find(this->Id);
			if (it != TextureHashtable.end()) {
				it->second.ref_count--;
				if (it->second.ref_count <= 0) {
					SDL_AtomicIncRef(&Reads);
					SDL_AtomicIncRef(&QueuedWrites);
					WriteTextures();
					LOG_DEBUG("erase texture");
					TextureHashtable.erase(it);
					SDL_AtomicDecRef(&Reads);
					return;
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
			this->Id = h.Id;
			IncrementRefCount();
		}

		Handle(Handle&& h) noexcept {
			this->Id = h.Id;
			IncrementRefCount();
		}

		Handle& operator=(const Handle& h) noexcept {
			this->Id = h.Id;
			IncrementRefCount();
			return *this;
		}

		Handle& operator=(Handle&& h) noexcept {
			this->Id = h.Id;
			IncrementRefCount();
			return *this;
		}

		inline uint32_t GetTexId() noexcept {
			ReadTextures();
			auto it = TextureHashtable.find(this->Id);
			if (it != TextureHashtable.end()) {
				SDL_AtomicIncRef(&Reads);
				return it->second.texId;
			}
			SDL_AtomicIncRef(&Reads);
			return 0;
		}

		inline void SetTexId(uint32_t Id) noexcept 
		{
			// same as read because nothing is inserted or erased
			ReadTextures(); 
			auto it = TextureHashtable.find(this->Id);
			if (it != TextureHashtable.end()) {
				it->second.texId = Id;
			}
			SDL_AtomicIncRef(&Reads);
		}
	};

	inline static Handle CreateOrGetTexture() noexcept {
		SDL_AtomicIncRef(&QueuedWrites);
		WriteTextures();
		TextureHashtable.insert(std::move(std::make_pair(TextureHashtable.size()+1, Texture{0, 0})));
		SDL_AtomicDecRef(&Reads);
		Handle handle(TextureHashtable.size());
		return handle;
	}

};

