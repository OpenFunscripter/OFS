#pragma once
#include "glad/gl.h"

#define OFS_InternalTexFormat GL_RGBA8
#ifdef WIN32
#define OFS_TexFormat GL_BGRA
#else
#define OFS_TexFormat GL_RGBA
#endif