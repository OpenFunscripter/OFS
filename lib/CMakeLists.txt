# ============
# == OPENGL ==
# ============
set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)

# ========
# = GLAD =
# ========
add_library(glad2 STATIC "glad2/src/gl.c")
target_include_directories(glad2 PUBLIC "glad2/include/")

# ==========
# == GLM ===
# ==========
add_subdirectory("glm")

# ==============
# == bitsery ===
# ==============
#add_library(bitsery INTERFACE)
#target_include_directories(bitsery INTERFACE "bitsery/include")
add_subdirectory("bitsery/")

# =============
# === EASTL ===
# =============
set(EASTL_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(EASTL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/EASTL/scripts/CMake/")
add_subdirectory("EASTL/")
add_subdirectory("EASTL/test/packages/EAStdC")
add_subdirectory("EASTL/test/packages/EAAssert")
add_subdirectory("EASTL/test/packages/EAThread")


# ====================
# == libserialport ===
# ====================
set(LIBSERIALPORT_SOURCES
	"libserialport/serialport.c"
	"libserialport/timing.c"
)
set(LIBSERIALPORT_HEADERS 
	"libserialport/libserialport.h"
	"libserialport/libserialport_internal.h"
)
if(WIN32)
set(LIBSERIALPORT_SOURCES
	${LIBSERIALPORT_SOURCES}
	"libserialport/windows.c"
)
elseif(UNIX AND NOT APPLE)
set(LIBSERIALPORT_SOURCES
	${LIBSERIALPORT_SOURCES}
	"libserialport/linux.c"
	"libserialport/linux_termios.c"
)
set(LIBSERIALPORT_HEADERS 
	${LIBSERIALPORT_HEADERS}
	"libserialport/linux_termios.h"
)
elseif(APPLE)
set(LIBSERIALPORT_SOURCES
	${LIBSERIALPORT_SOURCES}
	"libserialport/macosx.c"
)
endif()
add_library(libserialport STATIC ${LIBSERIALPORT_SOURCES} ${LIBSERIALPORT_HEADERS})
target_include_directories(libserialport PUBLIC "libserialport/")

if(WIN32)
target_compile_definitions(libserialport PUBLIC LIBSERIALPORT_MSBUILD)
else()
target_compile_definitions(libserialport PUBLIC 
	"LIBSERIALPORT_ATBUILD"
	"SP_PRIV="
	"SP_API=")
endif()

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
if(NOT WIN32)
	set_source_files_properties(${LUA_SOURCES} PROPERTIES LANGUAGE CXX)
	set_target_properties(lua PROPERTIES LINKER_LANGUAGE CXX)
	set_target_properties(lua PROPERTIES LANGUAGE CXX)
endif()
target_include_directories(lua PUBLIC "lua/")

set(LOVE_SRC_3P_LUASOCKET_ROOT
	luasocket/luasocket.cpp
	luasocket/luasocket.h
)

set(LOVE_SRC_3P_LUASOCKET_LIBLUASOCKET
	luasocket/libluasocket/auxiliar.c
	luasocket/libluasocket/auxiliar.h
	luasocket/libluasocket/buffer.c
	luasocket/libluasocket/buffer.h
	luasocket/libluasocket/compat.c
	luasocket/libluasocket/compat.h
	luasocket/libluasocket/except.c
	luasocket/libluasocket/except.h
	luasocket/libluasocket/ftp.lua.h
	luasocket/libluasocket/headers.lua.h
	luasocket/libluasocket/http.lua.h
	luasocket/libluasocket/inet.c
	luasocket/libluasocket/inet.h
	luasocket/libluasocket/io.c
	luasocket/libluasocket/io.h
	luasocket/libluasocket/ltn12.lua.h
	luasocket/libluasocket/luasocket.c
	luasocket/libluasocket/luasocket.h
	luasocket/libluasocket/mbox.lua.h
	luasocket/libluasocket/mime.c
	luasocket/libluasocket/mime.h
	luasocket/libluasocket/mime.lua.h
	luasocket/libluasocket/options.c
	luasocket/libluasocket/options.h
	luasocket/libluasocket/pierror.h
	luasocket/libluasocket/select.c
	luasocket/libluasocket/select.h
	luasocket/libluasocket/smtp.lua.h
	luasocket/libluasocket/socket.h
	luasocket/libluasocket/socket.lua.h
	luasocket/libluasocket/tcp.c
	luasocket/libluasocket/tcp.h
	luasocket/libluasocket/timeout.c
	luasocket/libluasocket/timeout.h
	luasocket/libluasocket/tp.lua.h
	luasocket/libluasocket/udp.c
	luasocket/libluasocket/udp.h
	luasocket/libluasocket/url.lua.h
)

set(LOVE_LINK_L3P_LUASOCKET_LIBLUASOCKET)

if(MSVC)
	set(LOVE_SRC_3P_LUASOCKET_LIBLUASOCKET
		${LOVE_SRC_3P_LUASOCKET_LIBLUASOCKET}
		luasocket/libluasocket/wsocket.c
		luasocket/libluasocket/wsocket.h
	)

	set(LOVE_LINK_L3P_LUASOCKET_LIBLUASOCKET
		${LOVE_LINK_L3P_LUASOCKET_LIBLUASOCKET}
		ws2_32.lib
	)
else()
	set(LOVE_SRC_3P_LUASOCKET_LIBLUASOCKET
		${LOVE_SRC_3P_LUASOCKET_LIBLUASOCKET}
		luasocket/libluasocket/serial.c
		luasocket/libluasocket/unix.c
		luasocket/libluasocket/unix.h
		luasocket/libluasocket/unixtcp.c
		luasocket/libluasocket/unixtcp.h
		luasocket/libluasocket/unixudp.c
		luasocket/libluasocket/unixudp.h
		luasocket/libluasocket/usocket.c
		luasocket/libluasocket/usocket.h
	)
endif()

set(LOVE_SRC_3P_LUASOCKET
	${LOVE_SRC_3P_LUASOCKET_ROOT}
	${LOVE_SRC_3P_LUASOCKET_LIBLUASOCKET}
)

add_library(love_3p_luasocket ${LOVE_SRC_3P_LUASOCKET})
target_link_libraries(love_3p_luasocket lua ${LOVE_LINK_L3P_LUASOCKET_LIBLUASOCKET})
target_include_directories(love_3p_luasocket PUBLIC "luasocket/")

#add_library(LuaJIT INTERFACE)
#target_include_directories(LuaJIT INTERFACE "LuaJIT/src")
#target_link_libraries(LuaJIT INTERFACE 
#	"${CMAKE_CURRENT_SOURCE_DIR}/LuaJIT/src/lua51.lib"
#)

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

#target_compile_definitions(imgui PUBLIC "ImDrawIdx=unsigned int") # TODO: not do this???
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
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(LIBC ON CACHE BOOL "" FORCE)
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
# === implot ===
# ============
set(implot_SOURCES
	"implot/implot.cpp"
	"implot/implot_demo.cpp"
	"implot/implot_items.cpp"
)
set(implot_HEADERS
	"implot/implot.h"
	"implot/implot_internal.h"
)
add_library(implot STATIC ${implot_SOURCES} ${implot_HEADERS})
target_include_directories(implot PUBLIC "implot")
target_link_libraries(implot PUBLIC imgui)
target_compile_features(implot PUBLIC cxx_std_17)


# =============
# === tracy ===
# =============
set(tracy_SOURCES
	"tracy/TracyClient.cpp"
)
set(tracy_HEADERS
	"tracy/Tracy.hpp"
)
add_library(tracy STATIC ${tracy_SOURCES} ${tracy_HEADERS})
target_include_directories(tracy PUBLIC "tracy/")
if(OFS_PROFILE)
target_compile_definitions(tracy PUBLIC "TRACY_ENABLE")
endif()

# ============
# = ImGuizmo =
# ============
file (GLOB ImGuizmo_SOURCES ImGuizmo/*.cpp)
file (GLOB ImGuizmo_HEADERS ImGuizmo/*.h)
add_library(ImGuizmo STATIC ${ImGuizmo_SOURCES} ${ImGuizmo_HEADERS})
target_include_directories(ImGuizmo PUBLIC "ImGuizmo/")
target_link_libraries(ImGuizmo PUBLIC imgui)
target_compile_features(ImGuizmo PUBLIC cxx_std_11)