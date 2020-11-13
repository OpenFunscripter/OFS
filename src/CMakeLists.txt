project(OpenFunscripter)


set(OPEN_FUNSCRIPTER_SOURCES
  "main.cpp"
  "OpenFunscripter.cpp"
  "UndoSystem.cpp"
  "Funscript/FunscriptAction.cpp"
  "Funscript/Funscript.cpp"
  "ControllerInput.cpp"
  "ScriptingMode.cpp"
  "OpenFunscripterUtil.cpp"
  "OFS_Serialization.cpp"

  "UI/OpenFunscripterSettings.cpp"
  "UI/KeybindingSystem.cpp"
  "UI/GradientBar.cpp"
  "UI/ScriptPositionsWindow.cpp"
  "UI/OpenFunscripterVideoplayer.cpp"
  "UI/ScriptSimulator.cpp"
  "UI/SpecialFunctions.cpp"

  "event/EventSystem.cpp"

  "imgui_impl/imgui_impl_opengl3.cpp"
  "imgui_impl/imgui_impl_sdl.cpp" 
)

if(WIN32)
	set(OPEN_FUNSCRIPTER_SOURCES ${OPEN_FUNSCRIPTER_SOURCES} "${CMAKE_SOURCE_DIR}/icon.rc")
endif()

add_executable(${PROJECT_NAME} ${OPEN_FUNSCRIPTER_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})

# copy data directory
add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy_directory
					${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:${PROJECT_NAME}>"/data")

target_link_libraries(${PROJECT_NAME} PUBLIC
  nlohmann_json
  imgui
  imgui_stdlib
  glad
  SDL2-static
  SDL2main
  tinyfiledialogs
  utf8::cpp
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
	"JSON_NOEXCEPTION"
)

if(WIN32)
	# copy mpv-1.dll to bin dir
	add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   "${CMAKE_SOURCE_DIR}/lib/libmpv/mpv-1.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>"/mpv-1.dll")
	target_include_directories(${PROJECT_NAME} PRIVATE 
		"${CMAKE_SOURCE_DIR}/lib/libmpv/include"
	)
	target_link_directories(${PROJECT_NAME} PUBLIC 
		"${CMAKE_SOURCE_DIR}/lib/libmpv/"
	)
	target_link_libraries(${PROJECT_NAME} PUBLIC
		# linking of libmpv can be improved but this works...
		  mpv.lib
	)
	target_compile_definitions(${PROJECT_NAME} PUBLIC
		"NOMINMAX"
	)
else()
# linux etc. 
find_package(PkgConfig REQUIRED) 
pkg_check_modules(mpv REQUIRED IMPORTED_TARGET mpv)

target_link_libraries(${PROJECT_NAME} PUBLIC
	mpv
	OpenGL
)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE 
	"${PROJECT_SOURCE_DIR}/"
	"${PROJECT_SOURCE_DIR}/imgui_impl/"
	"${PROJECT_SOURCE_DIR}/UI/"
	"${PROJECT_SOURCE_DIR}/event/"
	"${PROJECT_SOURCE_DIR}/Funscript/"
	"${CMAKE_SOURCE_DIR}/lib/minimp3/"
)

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
	"FUN_LATEST_GIT_TAG=${LATEST_GIT_TAG}"
	"FUN_LATEST_GIT_HASH=${LATEST_GIT_HASH}"
)


# c++17
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

# warnings as errors
if(MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC /EHc- /GR-)
	# target_compile_options(${PROJECT_NAME} PRIVATE /W2 /WX)
	# target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
else() # clang/gcc
	target_compile_options(${PROJECT_NAME} PUBLIC -fno-exceptions -fno-rtti)
	target_compile_options(${PROJECT_NAME} PUBLIC -fpermissive)
	# target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -Werror 
	# 	-Wno-error=unused-parameter 
	# 	-Wno-error=unused-variable 
	# 	-Wno-gnu-anonymous-struct
	# 	-Wno-nested-anon-types
	# ) 
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "bin/")

if(OFS_SNAP_IMAGE)
# this is awful
target_compile_definitions(${PROJECT_NAME} PRIVATE
	"OFS_SNAP_IMAGE")
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data DESTINATION "bin/")
endif()
