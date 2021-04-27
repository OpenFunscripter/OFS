project(OFS_lib)

set(OFS_LIB_SOURCES
	"event/EventSystem.cpp"
	"Funscript/Funscript.cpp"
	"Funscript/FunscriptAction.cpp"
	"Funscript/FunscriptUndoSystem.cpp"
	"Funscript/FunscriptHeatmap.cpp"

	"UI/GradientBar.cpp"
	"UI/OFS_ImGui.cpp"
	"UI/OFS_Videoplayer.cpp"
	"UI/KeybindingSystem.cpp"
	"UI/OFS_VideoplayerControls.cpp"
	"UI/OFS_Videopreview.cpp"
	"UI/OFS_BlockingTask.cpp"

	"UI/OFS_ScriptTimeline.cpp"
	"UI/ScriptPositionsOverlayMode.cpp"

	"UI/OFS_Waveform.cpp"

	"imgui_impl/imgui_impl_opengl3.cpp"
	"imgui_impl/imgui_impl_sdl.cpp" 

	"gl/OFS_Shader.cpp"
	"gl/OFS_Texture.cpp"

	"player/OFS_TCode.cpp"
	"player/OFS_TCodeChannel.cpp"
	"player/OFS_TCodeProducer.cpp"

	"OFS_AsyncIO.cpp"

	"OFS_UndoSystem.cpp"
	"OFS_ControllerInput.cpp"

	"OFS_Allocator.cpp"

	"OFS_Serialization.cpp"
	"OFS_Util.cpp"
	"OFS_Threadpool.cpp"
)

add_library(${PROJECT_NAME} STATIC ${OFS_LIB_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
	"${PROJECT_SOURCE_DIR}/event/"
	"${PROJECT_SOURCE_DIR}/Funscript/"
	"${PROJECT_SOURCE_DIR}/UI/"
	"${PROJECT_SOURCE_DIR}/imgui_impl/"
	"${PROJECT_SOURCE_DIR}/gl/"
	"${PROJECT_SOURCE_DIR}/im3d/"
	"${PROJECT_SOURCE_DIR}/"
	"${PROJECT_SOURCE_DIR}/player/"
	"../lib/stb/"
	"../lib/CSerial/"
	"../lib/minimp3/"
)

target_link_libraries(${PROJECT_NAME} PUBLIC
	SDL2-static
	SDL2main
	nlohmann_json
	glad2
	imgui
	imgui_stdlib
	tinyfiledialogs
	utf8::cpp
	libserialport
	glm
	implot
	ImGuizmo
	reproc++
	bitsery
	tracy
	EASTL
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

find_package(Git)
execute_process(
    COMMAND ${GIT_EXECUTABLE} --git-dir=${CMAKE_SOURCE_DIR}/.git describe --abbrev=0 --tags
    RESULT_VARIABLE result
    OUTPUT_VARIABLE LATEST_GIT_TAG)

execute_process(
    COMMAND ${GIT_EXECUTABLE} --git-dir=${CMAKE_SOURCE_DIR}/.git rev-parse --short HEAD
    RESULT_VARIABLE result
    OUTPUT_VARIABLE LATEST_GIT_HASH)


string(REGEX REPLACE "\n$" "" LATEST_GIT_TAG "${LATEST_GIT_TAG}")
set(LATEST_GIT_TAG "\"${LATEST_GIT_TAG}\"")
message("Compiling with git tag: ${LATEST_GIT_TAG}")

string(REGEX REPLACE "\n$" "" LATEST_GIT_HASH "${LATEST_GIT_HASH}")
set(LATEST_GIT_HASH "\"${LATEST_GIT_HASH}\"")
message("Compiling with git hash: ${LATEST_GIT_HASH}")

target_compile_definitions(${PROJECT_NAME} PUBLIC 
	"OFS_LATEST_GIT_TAG=${LATEST_GIT_TAG}"
	"OFS_LATEST_GIT_HASH=${LATEST_GIT_HASH}"
	"IMGUI_IMPL_OPENGL_LOADER_GLAD2"
)

if(OFS_BENCHMARKS)
	target_compile_definitions(${PROJECT_NAME} PUBLIC OFS_BENCHMARK_ENABLED=1)
else()
	target_compile_definitions(${PROJECT_NAME} PUBLIC OFS_BENCHMARK_ENABLED=0)
endif()

if(OFS_PROFILE)
	target_compile_definitions(${PROJECT_NAME} PUBLIC OFS_PROFILE_ENABLED=1)
else()
	target_compile_definitions(${PROJECT_NAME} PUBLIC OFS_PROFILE_ENABLED=0)
endif()


if(WIN32)
	target_include_directories(${PROJECT_NAME} PUBLIC 
		"../lib/libmpv/include"
	)
	target_link_directories(${PROJECT_NAME} PUBLIC 
		"../lib/libmpv/"
	)
	target_link_libraries(${PROJECT_NAME} PUBLIC
		# linking of libmpv can be improved but this works...
		  mpv.lib
	)
	target_compile_definitions(${PROJECT_NAME} PUBLIC
		"NOMINMAX"
	)
elseif(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
	# linux etc. 
	find_package(PkgConfig REQUIRED) 
	pkg_check_modules(mpv REQUIRED IMPORTED_TARGET mpv)

	target_link_libraries(${PROJECT_NAME} PUBLIC
		mpv
		OpenGL
	)
elseif(APPLE)
	target_include_directories(${PROJECT_NAME} PUBLIC
		"/opt/local/include/"
	)
	target_link_libraries(${PROJECT_NAME} PUBLIC
		"/opt/local/lib/libmpv.dylib"
	)
endif()