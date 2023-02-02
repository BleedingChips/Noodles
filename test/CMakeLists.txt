cmake_minimum_required(VERSION 3.20)

add_executable(
	NoodlesUnitTest
	UnitTest.cpp
)

target_compile_features(NoodlesUnitTest PUBLIC cxx_std_23)
target_include_directories(NoodlesUnitTest PUBLIC "../include/")

target_link_libraries(NoodlesUnitTest PUBLIC NoodlesLib)
