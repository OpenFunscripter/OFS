#pragma once
#define SQLITE_ORM_OPTIONAL_SUPPORTED
#include "sqlite_orm/sqlite_orm.h"

#include "OFS_Util.h"

#include "SDL_thread.h"
#include "SDL_atomic.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>


#define OFP_SQLITE_ID(id)\
int64_t id = -1

// shared_ptr because it doesn't cause the headache off removing copy constructors...
#define OFP_SQLITE_OPT_ID(id)\
std::optional<int64_t> id;

template<typename T>
struct Entity {
	auto insert();
	auto try_insert();
	void update();
	void replace();
};

struct Video;
struct Thumbnail;
struct Tag;
struct VideoAndTag;

struct Video : Entity<Video>
{
	OFP_SQLITE_ID(id);

	std::string filename;
	std::string path;

	uint64_t byte_count;
	uint64_t timestamp;

	bool hasScript;
	bool shouldGenerateThumbnail;

	OFP_SQLITE_OPT_ID(thumbnailId);
	std::optional<Thumbnail> thumbnail() const;
	inline bool HasThumbnail() const { return shouldGenerateThumbnail; }
};

struct Thumbnail : Entity<Thumbnail>
{
	OFP_SQLITE_ID(id);
	std::vector<char> thumb_buffer;
};

struct Tag : Entity<Tag>
{
	OFP_SQLITE_ID(id);
	std::string tag;

	// not used in db
	bool FilterActive = false;
};

struct VideoAndTag : Entity<VideoAndTag>
{
	OFP_SQLITE_ID(videoId);
	OFP_SQLITE_ID(tagId);
};

inline auto initStorage(const std::string& path)
{
	using namespace sqlite_orm;
	return make_storage(path,
		// video files
		make_table("videos",
			make_column("video_pk", &Video::id, autoincrement(), primary_key()),
			make_column("filename", &Video::filename),
			make_column("path", &Video::path, unique()),
			make_column("byte_count", &Video::byte_count),
			make_column("timestamp", &Video::timestamp),
			make_column("has_script", &Video::hasScript),
			make_column("gen_thumbnail", &Video::shouldGenerateThumbnail),
			make_column("thumbnail_fk", &Video::thumbnailId),
			foreign_key(&Video::thumbnailId).references(&Thumbnail::id)
		),

		// thumbnails
		make_table("thumbnails",
			make_column("thumb_pk", &Thumbnail::id, autoincrement(), primary_key()),
			make_column("image_buffer", &Thumbnail::thumb_buffer)
		),

		// tags
		make_table("tags",
			make_column("tag_pk", &Tag::id, autoincrement(), primary_key()),
			make_column("tag", &Tag::tag, unique())
		),

		// meta & tag
		make_table("videos_and_tags",
			make_column("video_fk", &VideoAndTag::videoId),
			make_column("tag_fk", &VideoAndTag::tagId),
			foreign_key(&VideoAndTag::videoId).references(&Video::id),
			foreign_key(&VideoAndTag::tagId).references(&Tag::id),
			primary_key(&VideoAndTag::videoId, &VideoAndTag::tagId)
		)
	);
}
using StorageT = decltype(initStorage(""));

class Videolibrary {
private:
	static StorageT Storage;

	static SDL_atomic_t Reads;
	static SDL_atomic_t QueuedWrites;
	static SDL_sem* WriteSem;

	inline static void ReadDB() noexcept {
		// all "reads" decrement the Reads variable
		// as long as it's not positive and there are no queuedWrites
		for (;;) {
			int value = SDL_AtomicGet(&Reads);
			int queuedWrites = SDL_AtomicGet(&QueuedWrites);
			if (value > 0 || queuedWrites > 0) { continue; }
			if (SDL_AtomicCAS(&Reads, value, value - 1)) {
				//LOGF_DEBUG("ReadDB: %d", value - 1);	
				break;
			}
		}
		// after reading SDL_AtomicIncRef(&Reads);
	}

	inline static void WriteDB() noexcept {
		SDL_AtomicIncRef(&QueuedWrites); // queue this write

		// grab all queuedWrites and set it to 0 atomically
		int queuedWrites;
		for (;;) {
			queuedWrites = SDL_AtomicGet(&QueuedWrites);
			if (SDL_AtomicCAS(&QueuedWrites, queuedWrites, 0)) { break; }
		}
		// if Reads is 0 set it attomically to queuedWrites
		for (;;) {
			int value = SDL_AtomicGet(&Reads);
			if (value < 0) { continue; }
			if (value > 0 && queuedWrites == 0) { break; }
			if (SDL_AtomicCAS(&Reads, 0, queuedWrites)) {
				//LOGF_DEBUG("WriteDB: %d", queuedWrites);
				break;
			}
		}
		// multiple writes have to go one at a time
		SDL_SemWait(WriteSem);
		// after writing SDL_AtomicDecRef(&Reads) + SDL_SemPost(WriteSem);
	}
	inline static void EndWriteDB() noexcept
	{
		SDL_AtomicDecRef(&Reads);
		SDL_SemPost(WriteSem);
	}
public:

	inline static void init() {
		Storage.sync_schema();
		Storage.open_forever();
		Storage.busy_timeout(15000);
	}

	inline static void DeleteAll() {
		WriteDB();
		Storage.remove_all<VideoAndTag>();
		Storage.remove_all<Tag>();
		Storage.remove_all<Video>();
		Storage.remove_all<Thumbnail>();
		Storage.vacuum();
		EndWriteDB();
	}

	inline static std::vector<Tag> GetTagsForVideo(int64_t videoId) {
		using namespace sqlite_orm;
		ReadDB();
		try {
			auto tagsJoin = Storage.get_all<Tag>(
				inner_join<VideoAndTag>(on(c(&VideoAndTag::tagId) == &Tag::id)),
				where(c(&VideoAndTag::videoId) == videoId)
			);
			SDL_AtomicIncRef(&Reads);
			return tagsJoin;
		}
		catch (std::system_error& er) {
			LOGF_ERROR("%s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return std::vector<Tag>();
	}

	inline static int64_t GetTagCountForVideo(int64_t videoId) {
		using namespace sqlite_orm;
		ReadDB();
		try 
		{
			auto count = Storage.count(
				&Tag::id,
				inner_join<Tag>(on(c(&Tag::id) == &VideoAndTag::tagId)),
				where(c(&VideoAndTag::videoId) == videoId)
			);
			SDL_AtomicIncRef(&Reads);
			return count;
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("%s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return 0;
	}

	inline static std::vector<Video> GetVideosWithTags(std::vector<int64_t>& tagIds) {
		using namespace sqlite_orm;		
		ReadDB();
		try {
			auto videos = Storage.get_all<Video>(
				inner_join<VideoAndTag>(on(c(&Video::id) == &VideoAndTag::videoId)),
				where(in(&VideoAndTag::tagId, tagIds) /*c(&VideoAndTag::tagId) == tagId*/),
				group_by(&Video::id)
			);
			SDL_AtomicIncRef(&Reads);
			return videos;
		}
		catch (std::system_error& er) {
			LOGF_ERROR("%s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return std::vector<Video>();
	}

	template<typename T>
	inline static std::optional<T> Get(const std::optional<int64_t> id) {
		ReadDB();
		if (id.has_value()) {
			try {
				auto value = Storage.get_optional<T>(id.value());
				SDL_AtomicIncRef(&Reads);
				return value;

			}
			catch (std::system_error& er)
			{
				LOGF_ERROR("%s", er.what());
			}
		}
		SDL_AtomicIncRef(&Reads);
		return std::optional<T>();
	}

	template<typename T>
	inline static std::optional<T> Get(int64_t id) {
		ReadDB();
		try
		{
			std::optional<T> value = Storage.get_optional<T>(id);
			SDL_AtomicIncRef(&Reads);
			return value;
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("%s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return std::optional<T>();
	}

	template<typename T>
	inline static std::vector<T> GetAll()
	{
		ReadDB();
		try
		{
			auto values = Storage.get_all<T>();
			SDL_AtomicIncRef(&Reads);
			return values;
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("%s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return std::vector<T>();
	}

	inline static std::optional<Tag> TagByName(const std::string& name) 
	{
		using namespace sqlite_orm;
		ReadDB();
		try
		{
			auto tags = Storage.get_all<Tag>(
				where(c(&Tag::tag) == name)
			);
			FUN_ASSERT(tags.size() <= 1, "the tag is supposed to be unique");
			if (tags.size() == 1) {
				SDL_AtomicIncRef(&Reads);
				return std::make_optional<Tag>(tags.front());
			}
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("%s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return std::optional<Tag>();
	}

	template<typename T>
	inline static int64_t Count() {
		ReadDB();
		try
		{
			int64_t count = Storage.count<T>();
			SDL_AtomicIncRef(&Reads);
			return count;
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("%s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return 0;
	}

	template<typename T>
	inline static int64_t Insert(T& o)
	{
		WriteDB();
		try
		{
			auto id = Storage.insert(o);
			EndWriteDB();
			return id;
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("%s", er.what());
		}
		EndWriteDB();
		return -1;
	}

	template<typename T>
	inline static void Update(T& o)
	{
		WriteDB();
		try
		{
			Storage.update(o);
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("%s", er.what());
		}
		EndWriteDB();
	}

	template<typename T>
	inline static void Replace(T& o)
	{
		WriteDB();
		try{
			Storage.replace(o);
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("%s", er.what());
		}
		EndWriteDB();
	}

	template<typename T, typename... Ids>
	inline static void Remove(Ids... ids)
	{
		WriteDB();
		try {
			Storage.remove<T>(std::forward<Ids>(ids)...);
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("%s", er.what());
		}
		EndWriteDB();
	}
};


template<typename T>
inline auto Entity<T>::insert()
{
	try {
		((T*)this)->id = Videolibrary::Insert(*(T*)this);
	}
	catch (std::system_error& er) {
		LOGF_ERROR("%s", er.what());
		FUN_ASSERT(false, er.what());
	}
}

template<typename T>
inline auto Entity<T>::try_insert()
{
	try {
		((T*)this)->id = Videolibrary::Insert(*(T*)this);
	}
	catch (std::system_error& er) {
		LOGF_ERROR("%s", er.what());
	}
}

template<typename T>
inline void Entity<T>::update()
{
	try {
		Videolibrary::Update(*(T*)this);
	}
	catch (std::system_error& er) {
		LOGF_ERROR("%s", er.what());
		FUN_ASSERT(false, er.what());
	}
}

template<typename T>
inline void Entity<T>::replace()
{
	try {
		Videolibrary::Replace(*(T*)this);
	}
	catch (std::system_error& er) {
		LOGF_ERROR("%s", er.what());
		FUN_ASSERT(false, er.what());
	}
}