cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(PICO_BOARD adafruit_feather_rp2040)

include(libraries/pico_sdk_import.cmake)

project(oled_test C CXX ASM)

pico_sdk_init()

add_executable(oled_test
    src/main.cpp
    libraries/oled.cpp
)

target_include_directories(oled_test PRIVATE include)
target_link_libraries(oled_test pico_stdlib hardware_spi hardware_gpio)

pico_enable_stdio_usb(oled_test 1)
pico_enable_stdio_uart(oled_test 0)

pico_add_extra_outputs(oled_test)
