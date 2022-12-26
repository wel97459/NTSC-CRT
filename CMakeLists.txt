cmake_minimum_required(VERSION 3.10)

if (MINGW32)
	set(CMAKE_SYSTEM_NAME Windows)
	SET(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
	SET(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
	SET(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
	set(CMAKE_RANLIB x86_64-w64-mingw32-ranlib)
	set(SDL2_INCLUDE_DIR /usr/x86_64-w64-mingw32/include)
	set(SDL2_LIBRARY /usr/x86_64-w64-mingw32/lib)
endif()

SET(CMAKE_CXX_STANDARD 11)
project(ntsc)

find_package(SDL2 REQUIRED)

include_directories(${SDL2_INCLUDE_DIRS})

add_executable(${CMAKE_PROJECT_NAME}
    ntsc_crt.c
	crt.c
)

target_link_libraries(${CMAKE_PROJECT_NAME} -lm ${SDL2_LIBRARIES})