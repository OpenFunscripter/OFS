cmake_minimum_required(VERSION 3.16)

project(scripter)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# =============
# == OPTIONS ==
# =============

# ====================
# === DEPENDENCIES ===
# ====================
add_subdirectory("lib/")
add_subdirectory("OFS-lib/")

# ==============
# ==== SRC ====
# ==============
add_subdirectory("src/")

# ==============
# ==== OFP ====
# ==============
add_subdirectory("player/")