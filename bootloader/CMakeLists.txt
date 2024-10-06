# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico CACHE STRING "Board type")
set(PICO_32BIT 1) # necessary for the .hex file to generate correctly on 2.0.0

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(bootloader C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add libraries
add_subdirectory(lib/FatFS/src)

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function
        -Wno-maybe-uninitialized
        -Os)

# Add executable. Default name is the project name, version 0.1
add_executable(bootloader main.cpp lib/sd/sd.cpp lib/sd/hw_config.c)

pico_set_program_name(bootloader "bootloader")
pico_set_program_version(bootloader "0.1a")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(bootloader 0)
pico_enable_stdio_usb(bootloader 1)

# Add the standard library to the build
target_link_libraries(bootloader
            pico_stdlib
        )

# Add the standard include files to the build
target_include_directories(bootloader PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/lib/FatFS/src
  ${CMAKE_CURRENT_LIST_DIR}/lib/sd
)

# Print memory usage at the end of the build
set_property(TARGET bootloader APPEND_STRING PROPERTY LINK_FLAGS
    "-Wl,--print-memory-usage"
)

# Add any user requested libraries
target_link_libraries(bootloader 
        hardware_dma
        hardware_flash
        hardware_watchdog
        no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
        )

pico_add_extra_outputs(bootloader)

