# src/CMakeLists.txt

# Main test application
add_executable(squid-test
    main.c
)

# Link the squid protocol library
target_link_libraries(squid-test squid)

# Enable debug symbols
target_compile_options(squid-test PRIVATE -g)
