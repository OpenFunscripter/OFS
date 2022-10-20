project(OFS_lib)

set(OFS_LIB_SOURCES
	"event/EventSystem.cpp"
	"Funscript/Funscript.cpp"
	"Funscript/FunscriptAction.cpp"
	"Funscript/FunscriptUndoSystem.cpp"
	"Funscript/FunscriptHeatmap.cpp"

	"UI/GradientBar.cpp"
	"UI/OFS_ImGui.cpp"
	"UI/KeybindingSystem.cpp"
	"UI/OFS_VideoplayerControls.cpp"
	"UI/OFS_Videopreview.cpp"
	"UI/OFS_BlockingTask.cpp"
	
	"UI/OFS_ScriptTimeline.cpp"
	"UI/ScriptPositionsOverlayMode.cpp"
	
	"UI/OFS_Waveform.cpp"
	
	"videoplayer/OFS_VideoplayerEvents.cpp"
	"videoplayer/OFS_VideoplayerWindow.cpp"
	"videoplayer/impl/OFS_MpvVideoplayer.cpp"

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

	"OFS_Serialization.cpp"
	"OFS_Util.cpp"
	"OFS_Threadpool.cpp"
	"OFS_FileLogging.cpp"
	"OFS_DynamicFontAtlas.cpp"
	"OFS_MpvLoader.cpp"

	"OFS_StringsGenerated.cpp"

	"localization/OFS_Localization.cpp"
)

add_library(${PROJECT_NAME} STATIC ${OFS_LIB_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
	"${PROJECT_SOURCE_DIR}/event/"
	"${PROJECT_SOURCE_DIR}/Funscript/"
	"${PROJECT_SOURCE_DIR}/UI/"
	"${PROJECT_SOURCE_DIR}/imgui_impl/"
	"${PROJECT_SOURCE_DIR}/gl/"
	"${PROJECT_SOURCE_DIR}/"
	"${PROJECT_SOURCE_DIR}/player/"
	"${PROJECT_SOURCE_DIR}/localization/"
	"${PROJECT_SOURCE_DIR}/videoplayer/"
	"${CMAKE_CURRENT_BINARY_DIR}"
)

add_custom_command(OUTPUT "OFS_StringsGenerated.cpp" "OFS_StringsGenerated.h"
                   COMMAND gen_localization ARGS "${CMAKE_SOURCE_DIR}/localization/localization.csv"
                   MAIN_DEPENDENCY "${CMAKE_SOURCE_DIR}/localization/localization.csv"
                   DEPENDS gen_localization
                   VERBATIM)

target_link_libraries(${PROJECT_NAME} PUBLIC
	SDL2-static
	SDL2main
	nlohmann_json
	glad2
	imgui
	imgui_stdlib
	tinyfiledialogs
	libserialport
	glm
	ImGuizmo
	bitsery
	refl-cpp
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
	"MPV_ENABLE_DEPRECATED=0"
)

if(OFS_PROFILE)
	target_compile_definitions(${PROJECT_NAME} PUBLIC OFS_PROFILE_ENABLED=1)
	target_link_libraries(${PROJECT_NAME} PUBLIC tracy)
	message("== ${PROJECT_NAME} - Profiling enabled.")
else()
	target_compile_definitions(${PROJECT_NAME} PUBLIC OFS_PROFILE_ENABLED=0)
endif()


if(WIN32)
	target_include_directories(${PROJECT_NAME} PUBLIC 
		"../lib/libmpv/include"
	)
	#target_link_directories(${PROJECT_NAME} PUBLIC 
	#	"../lib/libmpv/"
	#)
	#target_link_libraries(${PROJECT_NAME} PUBLIC
	#	# linking of libmpv can be improved but this works...
	#	mpv.lib
	#)
	target_compile_definitions(${PROJECT_NAME} PUBLIC
		"NOMINMAX"
	)
elseif(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
	# linux etc. 
	find_package(PkgConfig REQUIRED) 
	pkg_check_modules(mpv REQUIRED IMPORTED_TARGET mpv)
	
	# mpv is required for the headers
	# but not linked
	# instead libmpv.so.1 is loaded at runtime
	# this avoids linking issues with mpv and lua symbols
elseif(APPLE)
	execute_process(
		COMMAND brew --prefix mpv
		RESULT_VARIABLE BREW_MPV
		OUTPUT_VARIABLE BREW_MPV_PREFIX
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if (BREW_MPV EQUAL 0 AND EXISTS "${BREW_MPV_PREFIX}")
		message(STATUS "Found mpv installed by Homebrew at ${BREW_MPV_PREFIX}")
		set(MPV_INCLUDE "${BREW_MPV_PREFIX}/include")
		set(MPV_LIB "${BREW_MPV_PREFIX}/lib")
	endif()
	target_include_directories(${PROJECT_NAME} PUBLIC
		"${MPV_INCLUDE}"
	)
	#target_link_libraries(${PROJECT_NAME} PUBLIC
	#	"${MPV_LIB}/libmpv.dylib"
	#)
endif()