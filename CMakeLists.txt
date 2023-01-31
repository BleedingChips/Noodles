cmake_minimum_required(VERSION 3.20)

project(
	NoodlesUnitTest
	DESCRIPTION "UnitTest For NoodlesLib"
	VERSION 1.0
	LANGUAGES C CXX
)



if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	add_subdirectory("../Potato" "Potato")
endif()

add_subdirectory("src")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	add_subdirectory("test")
endif()