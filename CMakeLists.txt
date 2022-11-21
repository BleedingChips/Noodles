cmake_minimum_required(VERSION 3.20)

project(
	NoodlesUnitTest
	DESCRIPTION "UnitTest For NoodlesLib"
	VERSION 1.0
	LANGUAGES C CXX
)

add_subdirectory("src")
add_subdirectory("test")