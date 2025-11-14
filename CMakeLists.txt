# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

if(1)
# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
endif()
set(PICO_BOARD adafruit_feather_rp2040 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(adc_test C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()


# Add executable. Default name is the project name, version 0.1
add_executable(oled_test 
    test/oled_test.cpp 
    libraries/oled.cpp
)

pico_enable_stdio_uart(oled_test 0)
pico_enable_stdio_usb(oled_test 1)

target_link_libraries(oled_test
    pico_stdlib
    hardware_adc
    hardware_spi
)

# Add the standard include files to the build
target_include_directories(oled_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)
pico_add_extra_outputs(oled_test)


add_executable(adc_test
    test/adc_test.cpp
    libraries/adc.cpp
)
pico_enable_stdio_uart(adc_test 0)
pico_enable_stdio_usb(adc_test 1)

target_link_libraries(adc_test
    pico_stdlib
    hardware_adc
)
target_include_directories(adc_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)
pico_add_extra_outputs(adc_test)