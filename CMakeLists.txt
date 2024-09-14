cmake_minimum_required (VERSION 3.11)

project(gym)

set (CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

OPTION(USERDIR_RESOURCES "Allow using local userdirectory resources" ON)

set(CMAKE_CXX_FLAGS "-fpermissive -no-pie -ggdb")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
file (GLOB common_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  glad.c)
list(REMOVE_DUPLICATES common_sources)

# Add OpenGL
find_package(OpenGL)
link_libraries(${OpenGL_LIBRARIES})
include_directories(${OpenGL_INCLUDE_DIR})

# Add rendering external libs
find_library(GLFW3 glfw ${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/)
find_library(GLAD glad ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/)
find_library(GLM glm_static ${CMAKE_CURRENT_SOURCE_DIR}/external/glm/)
find_library(FREETYPE freetype ${CMAKE_CURRENT_SOURCE_DIR}/external/freetype/lib/)
find_library(ASSIMP assimp ${CMAKE_CURRENT_SOURCE_DIR}/external/assimp/) # :DeprecateMe This will be replaced with ufbx and cgltf

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/glm/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/freetype/include/freetype2/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/assimp/include/)
# For stb_image.h and other libs there
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/)

add_executable(game ${common_sources})
target_link_libraries(game
  ${GLFW3}
  ${GLAD}
  ${GLM}
  ${FREETYPE}
  ${ASSIMP}
  "-lX11"
  "-lGL"
  "-ldl"
  "-lpthread"
  "-lm"
)
