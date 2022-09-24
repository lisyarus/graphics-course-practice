cmake_minimum_required(VERSION 3.0)
project(practice4)

cmake_policy(SET CMP0072 NEW)
cmake_policy(SET CMP0074 NEW)

set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(SDL2 REQUIRED)

if(APPLE)
	# brew version of glew doesn't provide GLEW_* variables
	get_target_property(GLEW_INCLUDE_DIRS GLEW::GLEW INTERFACE_INCLUDE_DIRECTORIES)
	get_target_property(GLEW_LIBRARIES GLEW::GLEW INTERFACE_LINK_LIBRARIES)
	get_target_property(GLEW_LIBRARY GLEW::GLEW LOCATION)
	list(APPEND GLEW_LIBRARIES "${GLEW_LIBRARY}")
endif()

set(TARGET_NAME "${PROJECT_NAME}")

set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(${TARGET_NAME} main.cpp obj_parser.hpp obj_parser.cpp)
target_include_directories(${TARGET_NAME} PUBLIC
	"${SDL2_INCLUDE_DIRS}"
	"${GLEW_INCLUDE_DIRS}"
	"${OPENGL_INCLUDE_DIRS}"
)
target_link_libraries(${TARGET_NAME} PUBLIC
	"${GLEW_LIBRARIES}"
	"${SDL2_LIBRARIES}"
	"${OPENGL_LIBRARIES}"
)
target_compile_definitions(${TARGET_NAME} PUBLIC -DPROJECT_ROOT="${PROJECT_ROOT}")
