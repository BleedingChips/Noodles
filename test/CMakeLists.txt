cmake_minimum_required(VERSION 3.25.1)

ENABLE_CPP_MODULE()

add_executable(
	UnitTest
	UnitTest.cpp
)

target_compile_features(UnitTest PUBLIC cxx_std_23)
target_link_libraries(UnitTest PUBLIC NoodlesModule)
