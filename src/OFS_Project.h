#pragma once 

#include "OFS_BinarySerialization.h"
#include "OFS_ScriptSettings.h"
#include "Funscript/Funscript.h"

#include <vector>

#include "SDL_mutex.h"


class OFS_Project
{
	bool FindMedia(const std::string& funscriptPath) noexcept;
	void LoadScripts(const std::string& funscriptPath) noexcept;
	bool ImportFunscript(const std::string& path) noexcept;
public:
	static constexpr const char* Extension = ".OFS";

	std::string LastPath;
	bool Loaded = false;
	OFS_ScriptSettings Settings;
	std::vector<std::shared_ptr<Funscript>> Funscripts;
	std::string MediaPath;

	SDL_mutex* ProjectMut = nullptr;
	ByteBuffer ProjectBuffer;

	OFS_Project() noexcept;
	~OFS_Project() noexcept;

	void Clear() noexcept;
	void Load(const std::string& path) noexcept;

	void Save() noexcept { Save(LastPath); }
	void Save(const std::string& path) noexcept;

	void AddFunscript(const std::string& path) noexcept;
	void RemoveFunscript(int idx) noexcept;

	void Import(const std::string& path) noexcept;

	void ExportFunscript(const std::string& outputPath, int idx) noexcept;
	void ExportFunscripts(const std::string& outputPath) noexcept;
	
	template<typename S>
	void serialize(S& s)
	{
		s.ext(*this, bitsery::ext::Growable{},
			[](S& s, OFS_Project& o) {
				s.text1b(o.MediaPath, o.MediaPath.max_size());
			    s.object(o.Settings);
				s.container(o.Funscripts, 100, 
					[](S& s, std::shared_ptr<Funscript>& script) {
					s.ext(script, bitsery::ext::StdSmartPtr{});
				});
			});
	}
};
