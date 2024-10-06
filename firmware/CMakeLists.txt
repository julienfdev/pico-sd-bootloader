cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_BOARD pico CACHE STRING "Board type")
set(PICO_32BIT 1) # necessary for the .hex file to generate correctly

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(firmware C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()


# Add executable. Default name is the project name, version 0.1
add_executable(firmware main.cpp)

pico_set_program_name(firmware "firmware")
pico_set_program_version(firmware "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(firmware 0)
pico_enable_stdio_usb(firmware 1)

# Add the standard library to the build
target_link_libraries(firmware
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(firmware PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)

# Using a custom linker script - 128k flash offset, .boot2 commented out
pico_set_linker_script(firmware ${CMAKE_CURRENT_SOURCE_DIR}/memmap.ld)

pico_add_extra_outputs(firmware)

