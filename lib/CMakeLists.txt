# ============
# == OPENGL ==
# ============
#set(OpenGL_GL_PREFERENCE "GLVND")
#find_package(OpenGL REQUIRED)

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
#add_subdirectory("glm")

# =========
# = IMGUI =
# =========
file (GLOB IMGUI_SOURCES imgui/**.cpp)
file (GLOB IMGUI_HEADERS imgui/**.h)
add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUI_HEADERS})
# set_property(TARGET imgui PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(imgui PUBLIC "imgui/")
target_compile_definitions(imgui PUBLIC "IMGUI_USE_STB_SPRINTF")
target_include_directories(imgui PUBLIC "stb/")
target_compile_definitions(imgui PUBLIC "IMGUI_DEFINE_MATH_OPERATORS")

# ========
# = SDL2 =
# ========
add_subdirectory("SDL2/")


# =========================
# = portable-file-dialogs =
# =========================
add_subdirectory("portable-file-dialogs/")

# ==============
# === SPDLOG ===
# ==============
#add_subdirectory("spdlog")

# ==============
# === xxHash ===
# ==============
#add_library(xxHash STATIC "xxHash/xxhash.c")
#target_include_directories(xxHash PUBLIC "xxHash/")

# ============
# === json ===
# ============
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_MultipleHeaders ON CACHE BOOL "" FORCE)
add_subdirectory("json/")