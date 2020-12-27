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
	
	"UI/OFS_ScriptTimeline.cpp"
	"UI/ScriptPositionsOverlayMode.cpp"

	"imgui_impl/imgui_impl_opengl3.cpp"
	"imgui_impl/imgui_impl_sdl.cpp" 

	"gl/OFS_Shader.cpp"

	"im3d/OFS_im3d.cpp"
	"player/OFS_Tcode.cpp"
	"OFS_UndoSystem.cpp"
	"OFS_ControllerInput.cpp"

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
	"${PROJECT_SOURCE_DIR}/im3d/"
	"${PROJECT_SOURCE_DIR}/"
	"${PROJECT_SOURCE_DIR}/player/"
	"${CMAKE_SOURCE_DIR}/lib/stb/"
	"${CMAKE_SOURCE_DIR}/lib/CSerial/"
	"${CMAKE_SOURCE_DIR}/lib/minimp3/"
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
	cserial_static
	im3d
	glm
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)


if(WIN32)
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