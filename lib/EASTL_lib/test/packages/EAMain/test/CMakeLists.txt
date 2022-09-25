#-------------------------------------------------------------------------------------------
# Copyright (C) Electronic Arts Inc.  All rights reserved.
#-------------------------------------------------------------------------------------------

#-------------------------------------------------------------------------------------------
# CMake info
#-------------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.1)
project(EAMainTest CXX)
include(CTest)

#-------------------------------------------------------------------------------------------
# Defines
#-------------------------------------------------------------------------------------------
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_definitions(-D_CHAR16T)

#-------------------------------------------------------------------------------------------
# Compiler Flags
#-------------------------------------------------------------------------------------------
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/packages/EASTL/scripts/CMake")
include(CommonCppFlags)

#-------------------------------------------------------------------------------------------
# Source files
#-------------------------------------------------------------------------------------------
file(GLOB EAMAINTEST_SOURCES "source/Main/Main.cpp")
set(SOURCES ${EAMAINTEST_SOURCES})

#-------------------------------------------------------------------------------------------
# Executable definition
#-------------------------------------------------------------------------------------------
add_executable(EAMainTest ${SOURCES})

#-------------------------------------------------------------------------------------------
# Dependencies 
#-------------------------------------------------------------------------------------------
add_subdirectory(packages/EAAssert)
add_subdirectory(packages/EABase)
add_subdirectory(packages/EASTL)
add_subdirectory(packages/EAStdC)
add_subdirectory(packages/EATest)
add_subdirectory(packages/EAThread)

target_link_libraries(EAMainTest EABase)
target_link_libraries(EAMainTest EAAssert)
target_link_libraries(EAMainTest EAMain)
target_link_libraries(EAMainTest EASTL)
target_link_libraries(EAMainTest EAStdC)
target_link_libraries(EAMainTest EATest)
target_link_libraries(EAMainTest EAThread)

#-------------------------------------------------------------------------------------------
# Run Unit tests and verify the results.
#-------------------------------------------------------------------------------------------
add_test(EAMainTestRuns EAMainTest -testargpassing)
set_tests_properties (EAMainTestRuns PROPERTIES PASS_REGULAR_EXPRESSION "RETURNCODE=0")

