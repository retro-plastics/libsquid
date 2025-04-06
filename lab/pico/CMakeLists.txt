cmake_minimum_required(VERSION 3.13)

# initialize pico-sdk from GIT
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_FETCH_FROM_GIT on)

# set the right board
set(PICO_BOARD pico_w)

# pico_sdk_import.cmake is a single file copied from this SDK
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(squid C CXX ASM)

set(CMAKE_CSTANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(blinky
    blinky.c
)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(blinky
    pico_stdlib
    pico_cyw43_arch_none)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(blinky)
pico_enable_stdio_uart(blinky 1)