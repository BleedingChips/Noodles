cmake_minimum_required(VERSION 3.20)

add_library(
	NoodlesLib
	STATIC
	NoodlesMemory.cpp
)

if(NOT DEFINED Potato_FOUND)
	message(FATAL_ERROR "Noodle Require Potato")
endif()

target_compile_features(NoodlesLib PUBLIC cxx_std_23)
target_include_directories(NoodlesLib PUBLIC ${Potato_INCLUDE_DIRS})
target_include_directories(NoodlesLib PUBLIC "../include/")
target_link_libraries(NoodlesLib PUBLIC PotatoLib)