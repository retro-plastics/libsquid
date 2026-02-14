# lib/squid/CMakeLists.txt

add_library(squid
    init.c
    link.c
    burst.c
    socket.c
)

target_include_directories(squid
    PUBLIC  ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_SOURCE_DIR}/lib
)

target_compile_options(squid PRIVATE -Wall -Wextra -g)
