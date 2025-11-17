cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(pico_sdk_import.cmake)
project(i2s_test C CXX ASM)
pico_sdk_init()

add_executable(i2s_test
    lib/i2s.c
)

pico_generate_pio_header(i2s_test ${CMAKE_CURRENT_LIST_DIR}/i2s.pio)

target_sources(i2s_test PRIVATE src/main.cpp)

pico_enable_stdio_uart(i2s_test 0)
pico_enable_stdio_usb(i2s_test 1)

target_link_libraries(i2s_test pico_stdlib)

target_link_libraries(i2s_test
        hardware_i2c
        hardware_dma
        hardware_pio
        hardware_clocks
        )

pico_add_extra_outputs(i2s_test)