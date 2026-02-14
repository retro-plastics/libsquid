cmake_minimum_required(VERSION 3.10)
project(squid C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS_DEBUG "-g")

include_directories(include)

# squid protocol library
add_subdirectory(lib/squid)

# chat demo
add_executable(squid-chat src/main.c)
target_include_directories(squid-chat PRIVATE ${CMAKE_SOURCE_DIR}/lib)
target_link_libraries(squid-chat squid)

# test suite
enable_testing()
add_executable(squid-test tests/test_squid.c)
target_include_directories(squid-test PRIVATE ${CMAKE_SOURCE_DIR}/lib)
target_link_libraries(squid-test squid)
add_test(NAME squid-test COMMAND squid-test)
