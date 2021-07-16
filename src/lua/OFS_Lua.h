#pragma once

#ifdef WIN32
extern "C" {
	#include "lua.h"
	#include "lauxlib.h"
	#include "lualib.h"
}
#else
	// on linux we use c++ linkage to not conflict with mpv
	#include "lua.h"
	#include "lauxlib.h"
	#include "lualib.h"
#endif