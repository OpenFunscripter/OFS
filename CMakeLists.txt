cmake_minimum_required(VERSION 3.16)

project(scripter)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

if(EMSCRIPTEN)
    set(EMSCRIPTEN_FLAGS
       "-s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4 -s WASM=1 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s DISABLE_EXCEPTION_CATCHING=1 -s NO_EXIT_RUNTIME=0 -s USE_SDL=2 -s ASSERTIONS=1 --embed-file \"${CMAKE_SOURCE_DIR}/data\""
       
       #-s ALLOW_MEMORY_GROWTH=1\
       #-s USE_PTHREADS=1\
       #-s SAFE_HEAP=1\
       #-s FULL_ES2=1\
       #--profiling-funcs\
    )
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EMSCRIPTEN_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EMSCRIPTEN_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -s PTHREAD_POOL_SIZE=4") #-pthread -s PTHREAD_POOL_SIZE=4
endif()

# =============
# == OPTIONS ==
# =============

# ====================
# === DEPENDENCIES ===
# ====================
add_subdirectory("lib/")

# ==============
# ==== SRC ====
# ==============
add_subdirectory("src/")