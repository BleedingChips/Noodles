cmake_minimum_required(VERSION 3.20)

project(
	NoodlesUnitTest
	DESCRIPTION "UnitTest For NoodlesLib"
	VERSION 1.0
	LANGUAGES C CXX
)

set(GITHUB_DIR "../")

set(PotatoLib_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Potato/include)
set(PotatoLib_CMAKE_SUBDIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../Potato/src)

add_subdirectory(${PotatoLib_CMAKE_SUBDIRECTORY} "PotatoLib")

add_subdirectory("src")
add_subdirectory("test")