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
)


if(WIN32)
	set(OPEN_FUNSCRIPTER_SOURCES ${OPEN_FUNSCRIPTER_SOURCES} "../icon.rc")
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

# c++17
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

if(WIN32)
	# target_compile_options(${PROJECT_NAME} PRIVATE /W2 /WX)
	# target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(UNIX AND NOT APPLE) # clang/gcc
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
	target_compile_options(${PROJECT_NAME} PUBLIC -fpermissive)
	
	# Note Mac specific extension .app
	set(APPS "\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app")
endif()


if(OFS_SNAP_IMAGE)
# this is awful
target_compile_definitions(${PROJECT_NAME} PRIVATE
	"OFS_SNAP_IMAGE")
install(DIRECTORY "../data" DESTINATION "bin/")
endif()
