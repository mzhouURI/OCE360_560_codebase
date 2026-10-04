# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(A4_SD_Card C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# SD card + FAT filesystem library (carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico v3.6.2)
add_subdirectory(lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src build_fatfs)

# Add executable. Default name is the project name, version 0.1

add_executable(A4_SD_Card A4_SD_Card.c )

pico_set_program_name(A4_SD_Card "A4_SD_Card")
pico_set_program_version(A4_SD_Card "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(A4_SD_Card 0)
pico_enable_stdio_usb(A4_SD_Card 1)

# Add the standard library to the build
target_link_libraries(A4_SD_Card
        pico_stdlib hardware_adc no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)

# Add the standard include files to the build
target_include_directories(A4_SD_Card PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(A4_SD_Card)

