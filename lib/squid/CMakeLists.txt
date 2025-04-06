# lib/squid/CMakeLists.txt

# Create the squid protocol library
add_library(squid
    protocol.c
)

# Public headers are in the main include directory
target_include_directories(squid
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)

# Enable debug symbols
target_compile_options(squid PRIVATE -g)
