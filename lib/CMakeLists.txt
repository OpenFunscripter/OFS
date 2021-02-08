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
#SET(CSERIAL_VERSION_MAJOR 0)
#SET(CSERIAL_VERSION_MINOR 2)
#SET(CSERIAL_VERSION_PATCH 0)
#SET(CSERIAL_VERSION_STRING ${CSERIAL_VERSION_MAJOR}.${CSERIAL_VERSION_MINOR}.${CSERIAL_VERSION_PATCH})
#INCLUDE(CheckIncludeFiles)
#INCLUDE(GNUInstallDirs)
#CHECK_INCLUDE_FILES(linux/serial.h HAVE_LINUX_SERIAL)
#
#set(C_SERIAL_SOURCES
#	"CSerial/c_serial.c" )
#set(C_SERIAL_HEADERS 
#	"CSerial/c_serial.h"
#    "CSerial/simplelogger_defs.h")
#
#configure_file("CSerial/config.h.in" "CSerial/config.h")
#add_library(cserial_static STATIC ${C_SERIAL_SOURCES} ${C_SERIAL_HEADERS})
#target_include_directories(cserial_static PUBLIC "CSerial/" "${CMAKE_CURRENT_BINARY_DIR}/CSerial/")
#target_compile_definitions(cserial_static PUBLIC "-DCSERIAL_VERSION=${CSERIAL_VERSION_STRING}" )
#set_target_properties(cserial_static PROPERTIES VERSION ${CSERIAL_VERSION_STRING} SOVERSION ${CSERIAL_VERSION_MAJOR} )
#
#if(WIN32)
#	target_compile_definitions(cserial_static PUBLIC "-DCSERIAL_LIB")
#	target_compile_definitions(cserial_static PUBLIC CSERIAL_STATIC )
#endif(WIN32)
 
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
elseif(UNIX AND NOT UNIX)
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

# ============
# = ImGuizmo =
# ============
file (GLOB ImGuizmo_SOURCES ImGuizmo/*.cpp)
file (GLOB ImGuizmo_HEADERS ImGuizmo/*.h)
add_library(ImGuizmo STATIC ${ImGuizmo_SOURCES} ${ImGuizmo_HEADERS})
target_include_directories(ImGuizmo PUBLIC "ImGuizmo/")
target_link_libraries(ImGuizmo PUBLIC imgui)
target_compile_features(ImGuizmo PUBLIC cxx_std_11)