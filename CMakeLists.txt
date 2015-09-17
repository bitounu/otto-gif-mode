cmake_minimum_required(VERSION 2.8)
project(otto_gif_mode)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
message("CXX Flags: ${CMAKE_CXX_FLAGS}")
set(OTTO_GFX    "deps/otto-gfx")
set(OTTO_RUNNER "deps/otto-runner")
set(OTTO_UTILS  "deps/otto-utils")
set(ENTITYX     "deps/entityx")

set(ENTITYX_DT_TYPE float)
set(ENTITYX_BUILD_SHARED OFF CACHE BOOL "No shared libs plz")
set(CROSS_COMPILING ON) # Needed for EntityX's C++11 checks

add_subdirectory(${ENTITYX})
add_subdirectory(${OTTO_GFX})

include_directories(
  deps/Choreograph/src
  ${ENTITYX}
  ${OTTO_RUNNER}/src
  ${OTTO_UTILS}/src
  lib)

include_directories(SYSTEM "${VC_SDK}/include") # VideoCore

file(GLOB deps_src
  "deps/Choreograph/src/choreograph/*.cpp"
  "deps/otto-utils/src/*.cpp")
file(GLOB src "src/*.cpp")

set(mode_src ${deps_src} ${src})

set_source_files_properties(${mode_src} PROPERTIES COMPILE_FLAGS
  "-include make_unique.hpp -include algorithm")

add_library(otto_gif_mode MODULE ${mode_src})
target_link_libraries(otto_gif_mode otto_gfx entityx)

# Copy assets to the build directory
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets
  DEPENDS ${CMAKE_SOURCE_DIR}/assets
  COMMENT "Copying assets")
add_custom_target(otto_gif_assets ALL DEPENDS ${CMAKE_BINARY_DIR}/assets)
add_dependencies(otto_gif_mode otto_gif_assets)
