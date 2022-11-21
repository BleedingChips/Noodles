cmake_minimum_required(VERSION 3.20)

add_library(
	NoodlesLib
	STATIC
	NoodlesMemory.cpp
)

target_include_directories(NoodlesLib PUBLIC "../include")

include(FetchContent)

# FetchContent_Declare

FetchContent_Declare(
	PotatoLib
	GIT_REPOSITORY https://github.com/BleedingChips/Potato.git
)

FetchContent_MakeAvailable(PotatoLib)

target_link_libraries(NoodlesLib PUBLIC Potato::PotatoLib)