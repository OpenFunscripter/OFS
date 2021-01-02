#include "sqlite_orm/sqlite_orm.h"

#include "OFS_Util.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>


#define OFP_SQLITE_ID(id)\
int32_t id = -1

#define OFP_SQLITE_OPT_ID(id)\
std::unique_ptr<int32_t> id = nullptr;

namespace OFS{
	template<typename T>
	void Set(std::unique_ptr<T>& ptr, T& val) noexcept {
		if (ptr == nullptr) {
			ptr = std::make_unique<T>(val);
		}
		else
		{
			*ptr = val;
		}
	}
}


template<typename T>
struct Entity {
	void insert();
	void update();
	void remove();
};

struct Video;
struct Thumbnail;
struct Tag;
struct MetaAndTag;

struct Video : Entity<Video>
{
	OFP_SQLITE_ID(id);

	std::string filename;
	std::string path;

	uint64_t byte_count;
	uint64_t timestamp;

	OFP_SQLITE_OPT_ID(thumbnailId);
	std::unique_ptr<Thumbnail> thumbnail() const;
};

struct Thumbnail : Entity<Thumbnail>
{
	OFP_SQLITE_ID(id);
	std::string path;
};

struct Tag : Entity<Tag>
{
	OFP_SQLITE_ID(id);
	std::string tag;
};

struct MetaAndTag : Entity<MetaAndTag>
{
	OFP_SQLITE_ID(metaId);
	OFP_SQLITE_ID(tagId);
};

class Videolibrary {
public:
	static auto Storage() {
		using namespace sqlite_orm;
		auto storage = make_storage(Util::PrefpathOFP("library.sqlite"),
			// video files
			make_table("videos",
				make_column("id", &Video::id, autoincrement(), primary_key()),
				make_column("filename", &Video::filename),
				make_column("path", &Video::path),
				make_column("byte_count", &Video::byte_count),
				make_column("timestamp", &Video::timestamp),

				make_column("thumbnail_id", &Video::thumbnailId),
				foreign_key(&Video::thumbnailId).references(&Thumbnail::id)
			),

			// thumbnails
			make_table("thumbnails",
				make_column("id", &Thumbnail::id, autoincrement(), primary_key()),
				make_column("path", &Thumbnail::path)
			),

			// tags
			make_table("tags",
				make_column("id", &Tag::id, autoincrement(), primary_key()),
				make_column("tag", &Tag::tag, unique())
			),

			// meta & tag
			make_table("meta_and_tags",
				make_column("meta_id", &MetaAndTag::metaId),
				make_column("tag_id", &MetaAndTag::tagId),
				foreign_key(&MetaAndTag::metaId).references(&Video::id),
				foreign_key(&MetaAndTag::tagId).references(&Tag::id)
			)
		);
		try {
			storage.sync_schema();
		}
		catch (std::system_error& er) {
			LOGF_ERROR("%s", er.what());
		}



		return storage;
	}
};


template<typename T>
inline void Entity<T>::insert()
{
	((T*)this)->id = Videolibrary::Storage().insert(*(T*)this);
}

template<typename T>
inline void Entity<T>::update()
{
	Videolibrary::Storage().update(*(T*)this);
}

template<typename T>
inline void Entity<T>::remove()
{
	Videolibrary::Storage().remove(*(T*)this);
}