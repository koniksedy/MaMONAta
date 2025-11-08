cmake_minimum_required(VERSION 3.15)
project(MaMONAta LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

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

add_executable(${PROJECT_NAME} main.cc)

# target_include_directories(${PROJECT_NAME} PRIVATE
#   "${CMAKE_SOURCE_DIR}/extern/mata"
#   "${CMAKE_SOURCE_DIR}/extern/mata/include"
#   "${CMAKE_SOURCE_DIR}/extern/mata/src"
# )

target_link_libraries(${PROJECT_NAME} PRIVATE mona_lib libmata)
