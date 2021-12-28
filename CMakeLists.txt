cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project("createico")

add_executable(createico
	src/createico.cpp
)
target_include_directories(createico
	PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}
)
