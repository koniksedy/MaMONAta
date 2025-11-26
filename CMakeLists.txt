cmake_minimum_required(VERSION 3.15)
project(MaMONAta LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_C_FLAGS "-fgnu89-inline")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
# set(CMAKE_CXX_FLAGS_DEBUG "-fsanitize=address -g -O0")

# Add MONA library
#################################################################
file(GLOB MONA_LIB_C_SOURCES
  "${CMAKE_SOURCE_DIR}/extern/MONA/Lib/*.c"
  "${CMAKE_SOURCE_DIR}/extern/MONA/DFA/*.c"
  "${CMAKE_SOURCE_DIR}/extern/MONA/BDD/*.c"
  "${CMAKE_SOURCE_DIR}/extern/MONA/Mem/*.c")

add_library(mona_lib STATIC ${MONA_LIB_C_SOURCES})

target_include_directories(mona_lib PUBLIC
  "${CMAKE_SOURCE_DIR}/extern/MONA/Lib"
  "${CMAKE_SOURCE_DIR}/extern/MONA/DFA"
  "${CMAKE_SOURCE_DIR}/extern/MONA/BDD"
  "${CMAKE_SOURCE_DIR}/extern/MONA/Mem"
  "${CMAKE_SOURCE_DIR}/extern/MONA")

# Add Mata library
#################################################################
add_subdirectory(extern/mata)

# Add MaMONAta executable
#################################################################
# Collect all source files from the src folder
file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cc")
add_executable(${PROJECT_NAME} ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/include/mata-bridge"
  "${CMAKE_SOURCE_DIR}/extern/mona-bridge")

target_link_libraries(${PROJECT_NAME} PRIVATE mona_lib libmata)
