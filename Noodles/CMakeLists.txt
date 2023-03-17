cmake_minimum_required(VERSION 3.25.1)

ENABLE_CPP_MODULE()

add_library(
	NoodlesModule
	STATIC
	"NoodlesMemory.cpp"
)

target_sources(
	NoodlesModule PUBLIC
	FILE_SET cxx_modules TYPE CXX_MODULES
	FILES
		"NoodlesMemory.ixx"
)

target_compile_features(NoodlesModule PUBLIC cxx_std_23)
target_link_libraries(NoodlesModule PUBLIC PotatoModule)