# ============
# == OPENGL ==
# ============
set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)

# ========
# = GLFW =
# ========
#set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
#set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
#set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
#add_subdirectory("glfw/")

# ========
# = GLAD =
# ========
add_library(glad STATIC "glad/src/glad.c")
target_include_directories(glad PUBLIC "glad/include/")
# set_property(TARGET glad PROPERTY POSITION_INDEPENDENT_CODE ON)

# ==========
# == GLM ===
# ==========
add_subdirectory("glm")


# ==============
# == CSerial ===
# ==============
add_subdirectory("CSerial")


# ==========
# == LUA ===
# ==========
set (LUA_SOURCES 
	"lua/lauxlib.c"
	"lua/lbaselib.c"
	"lua/ltablib.c"
	"lua/lutf8lib.c"

	"lua/lmathlib.c"
	"lua/lstrlib.c"
	"lua/loadlib.c"
	"lua/loslib.c"
	"lua/liolib.c"
	"lua/ldblib.c"
	"lua/lcorolib.c"

	"lua/lapi.c"
	"lua/lstate.c"
	"lua/ldebug.c"
	"lua/lzio.c"
	"lua/llex.c"
	"lua/lvm.c"
	"lua/ldump.c"
	"lua/ltm.c"
	"lua/lstring.c"
	"lua/lopcodes.c"
	"lua/lobject.c"
	"lua/lmem.c"
	"lua/linit.c"
	"lua/ltable.c"
	"lua/lctype.c"
	"lua/ldo.c"
	"lua/lfunc.c"
	"lua/lundump.c"
	"lua/lgc.c"
	"lua/lparser.c"
	"lua/lcode.c"
)
set (LUA_HEADERS 
	"lua/lua.h"
	"lua/lauxlib.h"
	"lua/lualib.h"
)
add_library(lua STATIC ${LUA_SOURCES} ${LUA_HEADERS})
set_source_files_properties(${LUA_SOURCES} PROPERTIES LANGUAGE CXX)
set_target_properties(lua PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(lua PROPERTIES LANGUAGE CXX)
target_include_directories(lua PUBLIC "lua/")

# =========
# = IMGUI =
# =========
file (GLOB IMGUI_SOURCES imgui/**.cpp)
file (GLOB IMGUI_HEADERS imgui/**.h)
add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUI_HEADERS})
# set_property(TARGET imgui PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(imgui PUBLIC "imgui/")
#target_include_directories(imgui PUBLIC "/")
target_compile_definitions(imgui PUBLIC "IMGUI_USE_STB_SPRINTF")
target_include_directories(imgui PUBLIC "stb/")
target_compile_definitions(imgui PUBLIC "IMGUI_DEFINE_MATH_OPERATORS")
target_compile_definitions(imgui PUBLIC "IMGUI_DISABLE_OBSOLETE_FUNCTIONS")

target_compile_definitions(imgui PUBLIC "ImDrawIdx=unsigned int") # TODO: not do this???
#target_compile_definitions(imgui PUBLIC "IMGUI_DEBUG_PARANOID")

# ================
# = IMGUI_STDLIB =
# ================
add_library(imgui_stdlib STATIC "imgui/misc/cpp/imgui_stdlib.cpp" "imgui/misc/cpp/imgui_stdlib.h")
target_include_directories(imgui_stdlib PUBLIC "imgui/misc/cpp")
target_link_libraries(imgui_stdlib PUBLIC imgui)

# ========
# = SDL2 =
# ========
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory("SDL2/")


# ===================
# = tinifiledialogs = 
# ===================
set(tiny_SRC "tinyfiledialogs/tinyfiledialogs.c")
set(tiny_HEADER "tinyfiledialogs/tinyfiledialogs.h")
add_library(tinyfiledialogs STATIC ${tiny_SRC} ${tiny_HEADER})
target_include_directories(tinyfiledialogs PUBLIC "tinyfiledialogs/")


# ==========
# = utfcpp = 
# ==========
set(UTF8_TESTS OFF CACHE BOOL "" FORCE)
set(UTF8_INSTALL OFF CACHE BOOL "" FORCE)
set(UTF8_SAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("utfcpp/")

# ==============
# === SPDLOG ===
# ==============
#add_subdirectory("spdlog")

# ==============
# === xxHash ===
# ==============
add_library(xxHash STATIC "xxHash/xxhash.c")
target_include_directories(xxHash PUBLIC "xxHash/")

# ==============
# === reproc ===
# ==============
set(REPROC++ ON CACHE BOOL "" FORCE)
add_subdirectory("reproc/")


# ============
# === json ===
# ============
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_MultipleHeaders ON CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
add_subdirectory("json/")


# ============
# === im3d ===
# ============
set(im3d_SOURCES
	"im3d/im3d.cpp"
)
set(im3d_HEADERS
	"im3d/im3d.h"
	"im3d/im3d_config.h"
	"im3d/im3d_math.h"
)
add_library(im3d STATIC ${im3d_SOURCES} ${im3d_HEADERS})
target_include_directories(im3d PUBLIC "im3d")
target_link_libraries(im3d PUBLIC glm)
target_compile_features(im3d PUBLIC cxx_std_17)


# ===============
# === sqlite3 ===
# ===============
#add_subdirectory("sqlite-amalgamation/")
set(sqlite3_SOURCES
	"sqlite-amalgamation/sqlite3.c"
)
set(sqlite3_HEADERS
	"sqlite-amalgamation/sqlite3.h"
	"sqlite-amalgamation/sqlite3ext.h"
)
add_library(sqlite3 STATIC ${sqlite3_SOURCES} ${sqlite3_HEADERS})
target_compile_definitions(sqlite3 PUBLIC 
    SQLITE_DQS=0
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    SQLITE_LIKE_DOESNT_MATCH_BLOBS
    SQLITE_MAX_EXPR_DEPTH=0
    SQLITE_OMIT_DECLTYPE
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_PROGRESS_CALLBACK
    SQLITE_OMIT_SHARED_CACHE
    SQLITE_USE_ALLOCA
	SQLITE_THREADSAFE=2
)

# ==================
# === sqlite_orm ===
# ==================
add_library(sqlite_orm INTERFACE)
target_link_libraries(sqlite_orm INTERFACE sqlite3)
target_include_directories(sqlite_orm INTERFACE "sqlite_orm/include")

