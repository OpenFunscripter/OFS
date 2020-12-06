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
  
  "UI/ScriptSimulator.cpp"
  "UI/SpecialFunctions.cpp"
  "UI/ScriptPositionsOverlayMode.cpp"

  "event/EventSystem.cpp"

  "gl/OFS_Shader.cpp"
  
  "imgui_impl/imgui_impl_opengl3.cpp"
  "imgui_impl/imgui_impl_sdl.cpp" 
  )
  
  if(EMSCRIPTEN)
  set(OPEN_FUNSCRIPTER_SOURCES ${OPEN_FUNSCRIPTER_SOURCES} "emscripten/OFS_EMS_Player.cpp")
  else()
  set(OPEN_FUNSCRIPTER_SOURCES ${OPEN_FUNSCRIPTER_SOURCES} 
    "UI/OpenFunscripterVideoplayer.cpp"
    "gl/Simulator3D.cpp"
)
endif()

if(WIN32)
	set(OPEN_FUNSCRIPTER_SOURCES ${OPEN_FUNSCRIPTER_SOURCES} "${CMAKE_SOURCE_DIR}/icon.rc")
endif()

if(APPLE)
add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${OPEN_FUNSCRIPTER_SOURCES})
else()
add_executable(${PROJECT_NAME} ${OPEN_FUNSCRIPTER_SOURCES})
endif()
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})

# copy data directory
add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy_directory
					${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:${PROJECT_NAME}>"/data")

if(EMSCRIPTEN)
message("=================> Welcome to the Cum Zone")
target_link_libraries(${PROJECT_NAME} PUBLIC
  nlohmann_json
  imgui
  imgui_stdlib
  #SDL2-static
  #SDL2main
  tinyfiledialogs
  utf8::cpp
  glm
  ImGuizmo
  ImGuizmoQuat
)
else()
target_link_libraries(${PROJECT_NAME} PUBLIC
  nlohmann_json
  imgui
  imgui_stdlib
  glad
  SDL2-static
  SDL2main
  tinyfiledialogs
  utf8::cpp
  glm
  ImGuizmo
  ImGuizmoQuat
)
endif()


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
elseif(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
# linux etc. 
find_package(PkgConfig REQUIRED) 
pkg_check_modules(mpv REQUIRED IMPORTED_TARGET mpv)

target_link_libraries(${PROJECT_NAME} PUBLIC
	mpv
	OpenGL
)
elseif(EMSCRIPTEN)
#    set(EMSCRIPTEN_FLAGS
#       "-s WASM=1\
#       -s ERROR_ON_UNDEFINED_SYMBOLS=0\
#       -s USE_SDL=2\
#       -s FULL_ES2=1\
#       -s USE_PTHREADS=1\
#       --embed-file ${CMAKE_SOURCE_DIR}/data"
#    )
#    set(CMAKE_EXECUTABLE_SUFFIX ".html")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EMSCRIPTEN_FLAGS}")
#    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EMSCRIPTEN_FLAGS}")
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
elseif(APPLE)
target_include_directories(${PROJECT_NAME} PUBLIC
	"/opt/local/include/"
)
target_link_libraries(${PROJECT_NAME} PUBLIC
	"/opt/local/lib/libmpv.dylib"
)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE 
	"${PROJECT_SOURCE_DIR}/"
	"${PROJECT_SOURCE_DIR}/imgui_impl/"
	"${PROJECT_SOURCE_DIR}/UI/"
	"${PROJECT_SOURCE_DIR}/event/"
	"${PROJECT_SOURCE_DIR}/Funscript/"
	"${PROJECT_SOURCE_DIR}/gl/"
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
if(WIN32)
    target_compile_options(${PROJECT_NAME} PUBLIC /EHc- /GR-)
	# target_compile_options(${PROJECT_NAME} PRIVATE /W2 /WX)
	# target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(UNIX AND NOT APPLE) # clang/gcc
	target_compile_options(${PROJECT_NAME} PUBLIC -fno-rtti)
  target_compile_options(${PROJECT_NAME} PUBLIC -fpermissive)
  target_compile_options(${PROJECT_NAME} PRIVATE -g -fsanitize=undefined)
  target_link_options(${PROJECT_NAME} PRIVATE -g -fsanitize=undefined)

	# target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -Werror 
	# 	-Wno-error=unused-parameter 
	# 	-Wno-error=unused-variable 
	# 	-Wno-gnu-anonymous-struct
	# 	-Wno-nested-anon-types
	# )
	install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "bin/")
elseif(APPLE)
	target_compile_options(${PROJECT_NAME} PUBLIC -fno-rtti)
	target_compile_options(${PROJECT_NAME} PUBLIC -fpermissive)
	
	# Note Mac specific extension .app
	set(APPS "\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app")
endif()


if(OFS_SNAP_IMAGE)
# this is awful
target_compile_definitions(${PROJECT_NAME} PRIVATE
	"OFS_SNAP_IMAGE")
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data DESTINATION "bin/")
endif()
