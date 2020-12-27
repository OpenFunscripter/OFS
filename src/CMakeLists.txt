project(OpenFunscripter)


set(OPEN_FUNSCRIPTER_SOURCES
  "main.cpp"
  "OpenFunscripter.cpp"
  
  "Funscript/OFS_ScriptSettings.cpp"
  
  "UI/OpenFunscripterSettings.cpp"

  "ScriptingMode.cpp"
  
  "UI/ScriptSimulator.cpp"
  "UI/SpecialFunctions.cpp"
  
  "UI/OFS_ScriptPositionsOverlays.cpp"

  "event/OFS_Events.cpp"

  "gl/Simulator3D.cpp"
)


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
					"${CMAKE_SOURCE_DIR}/data/" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data")


target_link_libraries(${PROJECT_NAME} PUBLIC
  lua
  OFS_lib
)

if(WIN32)
	# copy mpv-1.dll to bin dir
	add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   "${CMAKE_SOURCE_DIR}/lib/libmpv/mpv-1.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/mpv-1.dll")
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC
	"JSON_NOEXCEPTION"
)

target_include_directories(${PROJECT_NAME} PRIVATE 
	"${PROJECT_SOURCE_DIR}/"
	"${PROJECT_SOURCE_DIR}/UI/"
	"${PROJECT_SOURCE_DIR}/Funscript/"
	"${PROJECT_SOURCE_DIR}/gl/"
	"${PROJECT_SOURCE_DIR}/lua/"
	"${PROJECT_SOURCE_DIR}/event/"
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

if(WIN32)
    target_compile_options(${PROJECT_NAME} PUBLIC /EHc- /GR-)
	# target_compile_options(${PROJECT_NAME} PRIVATE /W2 /WX)
	# target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(UNIX AND NOT APPLE) # clang/gcc
	target_compile_options(${PROJECT_NAME} PUBLIC -fno-rtti)
    target_compile_options(${PROJECT_NAME} PUBLIC -fpermissive)
    
	#target_compile_options(${PROJECT_NAME} PRIVATE -g -fsanitize=undefined)
    #target_link_options(${PROJECT_NAME} PRIVATE -g -fsanitize=undefined)

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
