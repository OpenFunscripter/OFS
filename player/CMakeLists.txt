project(OFP)

set(OFP_SOURCES
  "main.cpp"
  "OFP.cpp"

  "Videobrowser/OFP_Videobrowser.cpp"
  "Videobrowser/OFP_VideobrowserItem.cpp"
  "Videobrowser/OFP_Videopreview.cpp"

  "UI/OFP_Settings.cpp"
)

add_executable(${PROJECT_NAME} ${OFP_SOURCES})

target_link_libraries(${PROJECT_NAME} PUBLIC
	OFS_lib
	xxHash
	reproc++
)

target_include_directories(${PROJECT_NAME} PRIVATE 
	"${PROJECT_SOURCE_DIR}/"
	"${PROJECT_SOURCE_DIR}/UI"
	"${PROJECT_SOURCE_DIR}/Videobrowser"
	#"${CMAKE_SOURCE_DIR}/lib/minimp3/"
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