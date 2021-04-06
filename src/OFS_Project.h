#pragma once 

#include "OFS_BinarySerialization.h"
#include "OFS_ScriptSettings.h"
#include "Funscript/Funscript.h"

#include <vector>

#include "SDL_mutex.h"


enum OFS_Project_Version : int32_t
{
	One = 1
};


class OFS_Project
{
	bool FindMedia(const std::string& funscriptPath) noexcept;
	void LoadScripts(const std::string& funscriptPath) noexcept;
	bool ImportFunscript(const std::string& path) noexcept;
public:
	static constexpr const char* Extension = ".OFS";

	bool Valid = false;
	bool Loaded = false;
	std::string LastPath;
	OFS_ScriptSettings Settings;
	std::vector<std::shared_ptr<Funscript>> Funscripts;
	std::string MediaPath;

	SDL_mutex* ProjectMut = nullptr;
	ByteBuffer ProjectBuffer;

	OFS_Project() noexcept;
	~OFS_Project() noexcept;

	void Clear() noexcept;
	bool Load(const std::string& path) noexcept;

	void Save() noexcept { Save(LastPath); }
	void Save(const std::string& path) noexcept;

	void AddFunscript(const std::string& path) noexcept;
	void RemoveFunscript(int idx) noexcept;

	bool Import(const std::string& path) noexcept;

	void ExportFunscript(const std::string& outputPath, int idx) noexcept;
	void ExportFunscripts(const std::string& outputPath) noexcept;
	void ExportFunscripts() noexcept;

	template<typename S>
	void serialize(S& s)
	{
		s.ext(*this, bitsery::ext::Growable{},
			[](S& s, OFS_Project& o) {
				auto CurrentVersion = OFS_Project_Version::One;
				s.value4b(CurrentVersion);
				if (CurrentVersion != OFS_Project_Version::One) {
					o.Valid = false;
					return;
				}
				s.text1b(o.MediaPath, o.MediaPath.max_size());
			    s.object(o.Settings);
				s.container(o.Funscripts, 100, 
					[](S& s, std::shared_ptr<Funscript>& script) {
					s.ext(script, bitsery::ext::StdSmartPtr{});
				});
				o.Valid = true;
			});
	}
};
