cmake_minimum_required(VERSION 3.15)
project(MaMONAta LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

# Require the user to specify a build type for single-config generators.
# For multi-config generators (Visual Studio, Xcode) this is ignored.
if(NOT CMAKE_CONFIGURATION_TYPES)
  if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    message(FATAL_ERROR "CMAKE_BUILD_TYPE is not set. Configure with e.g.:\n  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release\nValid values: Debug, Release, RelWithDebInfo, MinSizeRel")
  endif()
endif()

# TIMING_ENABLED: prefer explicit cache var, fallback to environment variable if set
if(DEFINED ENV{TIMING_ENABLED} AND NOT DEFINED TIMING_ENABLED)
  set(TIMING_ENABLED $ENV{TIMING_ENABLED})
endif()
option(TIMING_ENABLED "Enable timing instrumentation (defines TIMING_ENABLED)" ON)

if(TIMING_ENABLED)
  add_compile_definitions(TIMING_ENABLED)
endif()

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

# Ensure C compiler for mona uses -fgnu89-inline (only for C + GNU/Clang)
target_compile_options(mona_lib PRIVATE
  $<$<AND:$<COMPILE_LANGUAGE:C>,$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>>:-fgnu89-inline>
)

# Add Mata library
#################################################################
add_subdirectory(extern/mata)

# Add MaMONAta library
#################################################################
# Collect all source files from the src folder
file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cc")
add_library(${PROJECT_NAME} STATIC ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/include/mata-bridge"
  "${CMAKE_SOURCE_DIR}/extern/mona-bridge")

target_link_libraries(${PROJECT_NAME} PUBLIC mona_lib libmata)

# add examples subdirectory
add_subdirectory(examples)
