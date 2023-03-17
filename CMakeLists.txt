cmake_minimum_required(VERSION 3.25.1)

project(
	Noodles
	DESCRIPTION "ECS FrameWork"
	VERSION 1.0
	LANGUAGES C CXX
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	add_subdirectory("../Potato" "Potato")
endif()

ENABLE_CMAKE_CPP_MODULE()

add_subdirectory("Noodles")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	add_subdirectory("Test")
endif()