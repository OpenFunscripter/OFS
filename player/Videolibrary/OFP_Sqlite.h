#pragma once
#include "sqlite_orm/sqlite_orm.h"

#include "OFS_Util.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>


#define OFP_SQLITE_ID(id)\
int64_t id = -1

// shared_ptr because it doesn't cause the headache off removing copy constructors...
#define OFP_SQLITE_OPT_ID(id)\
std::shared_ptr<int64_t> id = nullptr;

namespace OFS{
	template<typename T>
	void Set(std::shared_ptr<T>& ptr, T& val) noexcept {
		if (ptr == nullptr) {
			ptr = std::make_shared<T>(val);
		}
		else
		{
			*ptr = val;
		}
	}
}

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
	std::unique_ptr<Thumbnail> thumbnail() const;
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
public:
	static StorageT Storage;

	static void init() {
		Storage.sync_schema();
		Storage.open_forever();
	}

	static std::vector<Video> GetVideos() {
		return Videolibrary::Storage.get_all<Video>();
	}

	static std::vector<Tag> GetTagsForVideo(int64_t videoId) {
		using namespace sqlite_orm;

		try {
			auto tagsJoin = Storage.get_all<Tag>(
				inner_join<VideoAndTag>(on(c(&VideoAndTag::tagId) == &Tag::id)),
				where(c(&VideoAndTag::videoId) == videoId)
			);
			return tagsJoin;
		}
		catch (std::system_error& er) {
			LOGF_ERROR("%s", er.what());
		}
		return std::vector<Tag>();
	}

	static int64_t GetTagCountForVideo(int64_t videoId) {
		using namespace sqlite_orm;

		auto count = Storage.count(
			&Tag::id,
			inner_join<Tag>(on(c(&Tag::id) == &VideoAndTag::tagId)),
			where(c(&VideoAndTag::videoId) == videoId)
		);
		
		return count;
	}

	static std::vector<Video> GetVideosWithTag(int64_t tagId) {
		using namespace sqlite_orm;
		try {
			auto videos = Storage.get_all<Video>(
				inner_join<VideoAndTag>(on(c(&Video::id) == &VideoAndTag::videoId)),
				where(c(&VideoAndTag::tagId) == tagId)
			);
			return videos;
		}
		catch (std::system_error& er) {
			LOGF_ERROR("%s", er.what());
		}
		return std::vector<Video>();
	}

	static VideoAndTag GetConnect(int64_t videoId, int64_t tagId) {

	}
};


template<typename T>
inline auto Entity<T>::insert()
{
	try {
		((T*)this)->id = Videolibrary::Storage.insert(*(T*)this);
	}
	catch (std::system_error& er) {
		FUN_ASSERT(false, er.what());
		LOGF_ERROR("%s", er.what());
	}
}

template<typename T>
inline auto Entity<T>::try_insert()
{
	try {
		((T*)this)->id = Videolibrary::Storage.insert(*(T*)this);
	}
	catch (std::system_error& er) {
		LOGF_ERROR("%s", er.what());
	}
}

template<typename T>
inline void Entity<T>::update()
{
	try {
		Videolibrary::Storage.update(*(T*)this);
	}
	catch (std::system_error& er) {
		FUN_ASSERT(false, er.what());
		LOGF_ERROR("%s", er.what());
	}
}

template<typename T>
inline void Entity<T>::replace()
{
	try {
		Videolibrary::Storage.replace(*(T*)this);
	}
	catch (std::system_error& er) {
		FUN_ASSERT(false, er.what());
		LOGF_ERROR("%s", er.what());
	}
}