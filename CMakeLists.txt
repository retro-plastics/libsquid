cmake_minimum_required(VERSION 3.10)
project(squid C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS_DEBUG "-g")

# Add include directories
include_directories(include)

# Add the squid library
add_subdirectory(lib/squid)

# Build the test app
add_executable(squid-test src/main.c)
target_link_libraries(squid-test squid)
