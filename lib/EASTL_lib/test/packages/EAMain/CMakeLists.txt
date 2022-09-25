#-------------------------------------------------------------------------------------------
# Copyright (C) Electronic Arts Inc.  All rights reserved.
#-------------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.1)
project(EAMain CXX)

#-------------------------------------------------------------------------------------------
# Options
#-------------------------------------------------------------------------------------------
option(EAMAIN_BUILD_TESTS "Enable generation of build files for tests" OFF)

#-------------------------------------------------------------------------------------------
# Compiler Flags
#-------------------------------------------------------------------------------------------
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/test/packages/EASTL/scripts/CMake")
include(CommonCppFlags)

#-------------------------------------------------------------------------------------------
# Library definition
#-------------------------------------------------------------------------------------------
file(GLOB EAMAIN_SOURCES "source/*.cpp" "source/internal/*.cpp")
add_library(EAMain ${EAMAIN_SOURCES})

#-------------------------------------------------------------------------------------------
# Package Tests
#-------------------------------------------------------------------------------------------
if(EAMAIN_BUILD_TESTS)
    add_subdirectory(test)
endif()

#-------------------------------------------------------------------------------------------
# Defines
#-------------------------------------------------------------------------------------------
add_definitions(-D_CHAR16T)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DEAMAIN_NETWORK_CHANNEL_PORT=8080)

#-------------------------------------------------------------------------------------------
# Export Include Directories
#-------------------------------------------------------------------------------------------
target_include_directories(EAMain PUBLIC include)
target_include_directories(EAMain PUBLIC source)

#-------------------------------------------------------------------------------------------
# Package Dependencies 
#-------------------------------------------------------------------------------------------
target_link_libraries(EAMain EABase)
target_link_libraries(EAMain EAAssert)
target_link_libraries(EAMain EAStdC)

