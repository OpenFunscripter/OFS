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
	auto replace();
};

struct Video;
struct Thumbnail;
struct Tag;
struct VideoAndTag;

struct Video : Entity<Video>
{
	OFP_SQLITE_ID(id);

	std::string videoFilename;
	std::string videoPath;

	std::string scriptPath;

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
	int32_t UsageCount();
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
			make_column("video_filename", &Video::videoFilename),
			make_column("video_path", &Video::videoPath, unique()),
			make_column("script_path", &Video::scriptPath),
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
	//static StorageT Storage;

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
			OFS_PAUSE_INTRIN();
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
			OFS_PAUSE_INTRIN();
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
			OFS_PAUSE_INTRIN();
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

	inline static StorageT storage()
	{
		static auto cachedPath = Util::PrefpathOFP("library.sqlite");
		auto store = initStorage(cachedPath);
		store.open_forever();
		store.busy_timeout(5000);
		//ReadDB();
		//store.sync_schema_simulate();
		//SDL_AtomicIncRef(&Reads);
		return store;
	}
public:

	inline static void init() {
		auto store = storage();
		try
		{
			store.sync_schema();
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("%s", er.what());
		}
		//Storage.sync_schema();
		//Storage.open_forever();
		//Storage.busy_timeout(15000);
	}

	inline static void DeleteAll() {
		auto store = storage();
		WriteDB();
		store.remove_all<VideoAndTag>();
		store.remove_all<Tag>();
		store.remove_all<Video>();
		store.remove_all<Thumbnail>();
		store.vacuum();
		EndWriteDB();
	}

	inline static std::vector<Tag> GetTagsForVideo(int64_t videoId) {
		using namespace sqlite_orm;
		auto store = storage();
		ReadDB();
		try {
			auto tagsJoin = store.get_all<Tag>(
				inner_join<VideoAndTag>(on(c(&VideoAndTag::tagId) == &Tag::id)),
				where(c(&VideoAndTag::videoId) == videoId)
			);
			SDL_AtomicIncRef(&Reads);
			return tagsJoin;
		}
		catch (std::system_error& er) {
			LOGF_ERROR("sqlite: %s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return std::vector<Tag>();
	}

	inline static int64_t GetTagUsage(int64_t tagId) 
	{
		using namespace sqlite_orm;
		auto store = storage();
		ReadDB();
		int count = 0;
		try
		{
			count = store.count<VideoAndTag>(where(c(&VideoAndTag::tagId) == tagId));
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("sqlite: %s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return count;
	}

	inline static std::optional<Tag> GetTagByName(const std::string& name)
	{
		using namespace sqlite_orm;
		auto store = storage();
		ReadDB();
		std::optional<Tag> result;
		try
		{
			auto tags = store.get_all<Tag>(where(c(&Tag::tag) == name));
			if (tags.size() > 0) {
				result = std::make_optional<Tag>(tags.front());
			}
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("sqlite: %s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return result;
	}

	inline static int64_t GetTagCountForVideo(int64_t videoId) {
		using namespace sqlite_orm;
		auto store = storage();
		ReadDB();
		try 
		{
			auto count = store.count(
				&Tag::id,
				inner_join<Tag>(on(c(&Tag::id) == &VideoAndTag::tagId)),
				where(c(&VideoAndTag::videoId) == videoId)
			);
			SDL_AtomicIncRef(&Reads);
			return count;
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("sqlite: %s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return 0;
	}

	inline static std::vector<Video> GetVideosWithTags(std::vector<int64_t>& tagIds) {
		using namespace sqlite_orm;		
		auto store = storage();
		ReadDB();
		try {
			auto videos = store.get_all<Video>(
				inner_join<VideoAndTag>(on(c(&Video::id) == &VideoAndTag::videoId)),
				where(in(&VideoAndTag::tagId, tagIds) /*c(&VideoAndTag::tagId) == tagId*/),
				group_by(&Video::id)
			);
			SDL_AtomicIncRef(&Reads);
			return videos;
		}
		catch (std::system_error& er) {
			LOGF_ERROR("sqlite: %s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return std::vector<Video>();
	}

	inline static std::optional<Video> GetVideoByPath(const std::string& path)
	{
		using namespace sqlite_orm;
		auto store = storage();
		ReadDB();
		try
		{
			auto video = store.get_all<Video>(
					where(c(&Video::videoPath) == path)
				);
			if (video.size() > 0) {
				SDL_AtomicIncRef(&Reads);
				return std::make_optional<Video>(video.front());
			}
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("sqlite: %s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return std::optional<Video>();
	}

	template<typename T>
	inline static std::optional<T> Get(const std::optional<int64_t> id) {
		auto store = storage();
		ReadDB();
		if (id.has_value()) {
			try {
				auto foo = id.value();
				auto value = store.get_optional<T>(foo);
				SDL_AtomicIncRef(&Reads);
				return value;

			}
			catch (std::system_error& er)
			{
				LOGF_ERROR("sqlite: %s", er.what());
			}
		}
		SDL_AtomicIncRef(&Reads);
		return std::optional<T>();
	}

	template<typename T>
	inline static std::optional<T> Get(int64_t id) {
		auto store = storage();
		ReadDB();
		try
		{
			std::optional<T> value = store.get_optional<T>(id);
			SDL_AtomicIncRef(&Reads);
			return value;
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("sqlite: %s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return std::optional<T>();
	}

	template<typename T>
	inline static std::vector<T> GetAll()
	{
		auto store = storage();
		ReadDB();
		try
		{
			auto values = store.get_all<T>();
			SDL_AtomicIncRef(&Reads);
			return values;
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("sqlite: %s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return std::vector<T>();
	}

	inline static std::optional<Tag> TagByName(const std::string& name) 
	{
		using namespace sqlite_orm;
		auto store = storage();
		ReadDB();
		try
		{
			auto tags = store.get_all<Tag>(
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
			LOGF_ERROR("sqlite: %s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return std::optional<Tag>();
	}

	template<typename T>
	inline static int64_t Count() {
		auto store = storage();
		ReadDB();
		try
		{
			int64_t count = store.count<T>();
			SDL_AtomicIncRef(&Reads);
			return count;
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("sqlite: %s", er.what());
		}
		SDL_AtomicIncRef(&Reads);
		return 0;
	}

	template<typename T>
	inline static int64_t Insert(T& o)
	{
		auto store = storage();
		WriteDB();
		try
		{
			auto id = store.insert(o);
			EndWriteDB();
			return id;
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("sqlite: %s", er.what());
		}
		EndWriteDB();
		return -1;
	}

	template<typename T>
	inline static void Update(T& o)
	{
		auto store = storage();
		WriteDB();
		try
		{
			store.update(o);
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("sqlite: %s", er.what());
		}
		EndWriteDB();
	}

	template<typename T>
	inline static bool Replace(T& o)
	{
		bool suc;
		auto store = storage();
		WriteDB();
		try{
			store.replace(o);
			suc = true;
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("sqlite: %s", er.what());
			suc = false;
		}
		EndWriteDB();
		return suc;
	}

	template<typename T, typename... Ids>
	inline static void Remove(Ids... ids)
	{
		auto store = storage();
		WriteDB();
		try {
			store.remove<T>(std::forward<Ids>(ids)...);
		}
		catch (std::system_error& er)
		{
			LOGF_ERROR("sqlite: %s", er.what());
		}
		EndWriteDB();
	}

	static bool WritebackFunscriptTags(int64_t videoId, const std::string& funscriptPath) noexcept;
};


template<typename T>
inline auto Entity<T>::insert()
{
	((T*)this)->id = Videolibrary::Insert(*(T*)this);
}

template<typename T>
inline auto Entity<T>::try_insert()
{
	((T*)this)->id = Videolibrary::Insert(*(T*)this);
	return ((T*)this)->id != -1;
}

template<typename T>
inline void Entity<T>::update()
{
	Videolibrary::Update(*(T*)this);
}

template<typename T>
inline auto Entity<T>::replace()
{
	return Videolibrary::Replace(*(T*)this);
}