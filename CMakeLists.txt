cmake_minimum_required(VERSION 3.25)

set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER "/usr/bin/arm-none-eabi-gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/arm-none-eabi-g++")
set(CMAKE_LINKER "/usr/arm-none-eabi/bin/as")
set(CMAKE_LINKER "/usr/arm-none-eabi/bin/ld")

set(CMAKE_FIND_ROOT_PATH /usr/arm-none-eabi)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

project(squid LANGUAGES C CXX ASM)

add_subdirectory(lib)
add_subdirectory(src)