project(squid)

# Set PICO SDK path.
set(PICO_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../lib/pico-sdk)

# Include the Pico SDK CMake functions
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

# Initialize the Pico SDK
pico_sdk_init()

# Add your own executable or library targets
add_executable(squid main.cpp)

# Link your targets to the Pico SDK libraries
target_link_libraries(squid pico_stdlib)