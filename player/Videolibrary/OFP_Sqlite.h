#pragma once
#include "sqlite_orm/sqlite_orm.h"

#include "OFS_Util.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>


#define OFP_SQLITE_ID(id)\
int32_t id = -1

// shared_ptr because it doesn't cause the headache off removing copy constructors...
#define OFP_SQLITE_OPT_ID(id)\
std::shared_ptr<int32_t> id = nullptr;

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
	T* insert();
	void update();
	void remove();
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
	//std::string path;
	std::vector<char> thumb_buffer;
};

struct Tag : Entity<Tag>
{
	OFP_SQLITE_ID(id);
	std::string tag;
};

struct VideoAndTag : Entity<VideoAndTag>
{
	OFP_SQLITE_ID(metaId);
	OFP_SQLITE_ID(tagId);
};

class Videolibrary {
public:
	static auto& Storage() {
		using namespace sqlite_orm;
		static auto storage = make_storage(Util::PrefpathOFP("library.sqlite"),
			// video files
			make_table("videos",
				make_column("id", &Video::id, autoincrement(), primary_key()),
				make_column("filename", &Video::filename),
				make_column("path", &Video::path, unique()),
				make_column("byte_count", &Video::byte_count),
				make_column("timestamp", &Video::timestamp),
				make_column("has_script", &Video::hasScript),
				make_column("gen_thumbnail", &Video::shouldGenerateThumbnail),
				make_column("thumbnail_id", &Video::thumbnailId),
				foreign_key(&Video::thumbnailId).references(&Thumbnail::id)
			),

			// thumbnails
			make_table("thumbnails",
				make_column("id", &Thumbnail::id, autoincrement(), primary_key()),
				make_column("image_buffer", &Thumbnail::thumb_buffer)
			),

			// tags
			make_table("tags",
				make_column("id", &Tag::id, autoincrement(), primary_key()),
				make_column("tag", &Tag::tag, unique())
			),

			// meta & tag
			make_table("videos_and_tags",
				make_column("meta_id", &VideoAndTag::metaId),
				make_column("tag_id", &VideoAndTag::tagId),
				foreign_key(&VideoAndTag::metaId).references(&Video::id),
				foreign_key(&VideoAndTag::tagId).references(&Tag::id)
			)
		);
		try {
			if (!storage.is_opened()) {
				storage.sync_schema();
				storage.open_forever();
			}
		}
		catch (std::system_error& er) {
			LOGF_ERROR("%s", er.what());
		}

		return storage;
	}

	static std::vector<Video> GetVideos() {
		return Videolibrary::Storage().get_all<Video>();
	}
};


template<typename T>
inline T* Entity<T>::insert()
{
	try {
		((T*)this)->id = Videolibrary::Storage().insert(*(T*)this);
		return ((T*)this);
	}
	catch (std::system_error& er) {
		LOGF_ERROR("%s", er.what());
	}
	return nullptr;
}

template<typename T>
inline void Entity<T>::update()
{
	try {
		Videolibrary::Storage().update(*(T*)this);
	}
	catch (std::system_error& er) {
		LOGF_ERROR("%s", er.what());
	}
}

template<typename T>
inline void Entity<T>::remove()
{
	try {
		Videolibrary::Storage().remove(*(T*)this);
	}
	catch (std::system_error& er) {
		LOGF_ERROR("%s", er.what());
	}
}