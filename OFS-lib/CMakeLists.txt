project(OFS_lib)

set(OFS_LIB_SOURCES
	"event/EventSystem.cpp"

	"OFS_Util.cpp"
)

add_library(${PROJECT_NAME} STATIC ${OFS_LIB_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
	"${PROJECT_SOURCE_DIR}/event/"
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
