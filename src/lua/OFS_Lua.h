#pragma once

// we're compiling and linking lua as c++ which is why no extern "C" is needed here
//extern "C" {
	#include "lua.h"
	#include "lauxlib.h"
	#include "lualib.h"
//}
