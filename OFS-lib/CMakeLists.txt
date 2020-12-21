project(OFS_lib)

set(OFS_LIB_SOURCES
	"event/EventSystem.cpp"
	"Funscript/Funscript.cpp"
	"Funscript/FunscriptAction.cpp"
	"Funscript/FunscriptUndoSystem.cpp"

	"UI/GradientBar.cpp"
	"UI/OFS_ImGui.cpp"
	"UI/OFS_Videoplayer.cpp"

	"imgui_impl/imgui_impl_opengl3.cpp"
	"imgui_impl/imgui_impl_sdl.cpp" 

	"gl/OFS_Shader.cpp"

	"OFS_Serialization.cpp"
	"OFS_Util.cpp"
)

add_library(${PROJECT_NAME} STATIC ${OFS_LIB_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
	"${PROJECT_SOURCE_DIR}/event/"
	"${PROJECT_SOURCE_DIR}/Funscript/"
	"${PROJECT_SOURCE_DIR}/UI/"
	"${PROJECT_SOURCE_DIR}/imgui_impl/"
	"${PROJECT_SOURCE_DIR}/gl/"
	"${PROJECT_SOURCE_DIR}/"
	"${CMAKE_SOURCE_DIR}/lib/stb/"
)

target_link_libraries(${PROJECT_NAME} PUBLIC
	SDL2-static
	SDL2main
	nlohmann_json
	glad
	imgui
	imgui_stdlib
	tinyfiledialogs
	utf8::cpp
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)


if(WIN32)
	# copy mpv-1.dll to bin dir
	add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   "${CMAKE_SOURCE_DIR}/lib/libmpv/mpv-1.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/mpv-1.dll")
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
elseif(APPLE)
target_include_directories(${PROJECT_NAME} PUBLIC
	"/opt/local/include/"
)
target_link_libraries(${PROJECT_NAME} PUBLIC
	"/opt/local/lib/libmpv.dylib"
)
endif()