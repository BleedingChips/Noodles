cmake_minimum_required(VERSION 3.20)

add_executable(
	NoodlesUnitTest
	UnitTest.cpp
)

target_link_libraries(NoodlesUnitTest PUBLIC NoodlesLib)