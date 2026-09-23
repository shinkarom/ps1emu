cmake_minimum_required(VERSION 3.13.2)

set(PROJ_NAME ps1emu)
project(${PROJ_NAME} CXX C)
set(EXE_NAME ${PROJ_NAME})
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include_directories(
	src
	vendor/glad/include
	)

add_executable(${PROJ_NAME}
	src/main.cpp
	src/bus.cpp
	src/core.cpp 
	src/cpu.cpp
	src/gte.cpp
	src/gpu.cpp
	src/spu.cpp
	src/dma.cpp
	src/cd.cpp
	src/mdec.cpp
)

#add_subdirectory(vendor/glfw)
add_subdirectory(vendor/raylib)
target_link_libraries(${PROJ_NAME} raylib)

set_target_properties(${PROJ_NAME} PROPERTIES
	LINKER_LANGUAGE CXX
)

target_compile_options(${EXE_NAME} PUBLIC 
	-g -Wall 
	$<$<CONFIG:Debug>:-g> 
	$<$<CONFIG:Release>: -O2>
)
target_link_options(${EXE_NAME} PUBLIC 
	$<$<CONFIG:RELEASE>: -s>
)
