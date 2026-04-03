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

# name project the name of executable being run/tested
project(module_FUNCTION C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

########## MAIN .CPP FILES ##########

### AUDIO INPUT MODULE ###
add_executable(module_INPUT
    src/module/input.cpp
    src/lib/I2S.cpp
    src/lib/RxPingPong.cpp
    src/lib/TxPingPong.cpp
    src/lib/adc.cpp
)
pico_generate_pio_header(module_INPUT ${CMAKE_CURRENT_LIST_DIR}/src/lib/pio/I2S_Tx_naive.pio)
pico_generate_pio_header(module_INPUT ${CMAKE_CURRENT_LIST_DIR}/src/lib/pio/I2S_Tx_compact.pio)
pico_generate_pio_header(module_INPUT ${CMAKE_CURRENT_LIST_DIR}/src/lib/pio/I2S_Rx_naive.pio)

pico_enable_stdio_uart(module_INPUT 0)
pico_enable_stdio_usb(module_INPUT 1)

target_link_libraries(module_INPUT
    pico_stdlib
    hardware_pio
    hardware_adc
    hardware_dma
    hardware_irq
)
target_include_directories(module_INPUT PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)
pico_add_extra_outputs(module_INPUT)

### AUDIO DSP MODULE ###
add_executable(module_FUNCTION
    src/module/function.cpp
    src/lib/I2S.cpp
    src/lib/RxPingPong.cpp
    src/lib/TxPingPong.cpp
    src/lib/adc.cpp
    src/lib/oled.cpp
    src/lib/fft.cpp
)
pico_generate_pio_header(module_FUNCTION ${CMAKE_CURRENT_LIST_DIR}/src/lib/pio/I2S_Tx_naive.pio)
pico_generate_pio_header(module_FUNCTION ${CMAKE_CURRENT_LIST_DIR}/src/lib/pio/I2S_Tx_compact.pio)
pico_generate_pio_header(module_FUNCTION ${CMAKE_CURRENT_LIST_DIR}/src/lib/pio/I2S_Rx_naive.pio)

pico_enable_stdio_uart(module_FUNCTION 0)
pico_enable_stdio_usb(module_FUNCTION 1)

target_link_libraries(module_FUNCTION
    pico_stdlib
    pico_multicore
    pico_util
    hardware_pio
    hardware_adc
    hardware_dma
    hardware_irq
    hardware_spi
)
target_include_directories(module_FUNCTION PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)
pico_add_extra_outputs(module_FUNCTION)

### AUDIO OUTPUT MODULE ###
add_executable(module_OUTPUT
    src/module/output.cpp
    src/lib/I2S.cpp
    src/lib/RxPingPong.cpp
    src/lib/adc.cpp
)
pico_generate_pio_header(module_OUTPUT ${CMAKE_CURRENT_LIST_DIR}/src/lib/pio/I2S_Tx_naive.pio)
pico_generate_pio_header(module_OUTPUT ${CMAKE_CURRENT_LIST_DIR}/src/lib/pio/I2S_Tx_compact.pio)
pico_generate_pio_header(module_OUTPUT ${CMAKE_CURRENT_LIST_DIR}/src/lib/pio/I2S_Rx_naive.pio)

pico_enable_stdio_uart(module_OUTPUT 0)
pico_enable_stdio_usb(module_OUTPUT 1)

target_link_libraries(module_OUTPUT
    pico_stdlib
    hardware_pio
    hardware_adc
    hardware_dma
    hardware_irq
)
target_include_directories(module_OUTPUT PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)
pico_add_extra_outputs(module_OUTPUT)

if(BUILD_TESTS)

######### GENERAL ##########
add_executable(general_test
    src/test/general_test.cpp
    src/lib/adc.cpp
    src/lib/I2S.cpp
    src/lib/RxPingPong.cpp
)

pico_enable_stdio_uart(general_test 0)
pico_enable_stdio_usb(general_test 1)

target_link_libraries(general_test
    pico_stdlib
    hardware_adc
    hardware_irq
    hardware_pio
    hardware_dma
    pico_float
)
target_include_directories(general_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)
pico_add_extra_outputs(general_test)

######### OLED ##########
add_executable(oled_test
    src/test/oled_test.cpp
    src/lib/oled.cpp
)

pico_enable_stdio_uart(oled_test 0)
pico_enable_stdio_usb(oled_test 1)

target_link_libraries(oled_test
    pico_stdlib
    hardware_adc
    hardware_spi
)
target_include_directories(oled_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)
pico_add_extra_outputs(oled_test)

########## ADC ##########
add_executable(adc_test
    src/test/adc_test.cpp
    src/lib/adc.cpp
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

########## I2S / PIO ##########
add_executable(I2S_Tx_test
    src/test/I2S_Tx_test.cpp
    src/lib/I2S.cpp
    src/lib/TxPingPong.cpp
)
pico_generate_pio_header(I2S_Tx_test ${CMAKE_CURRENT_LIST_DIR}/src/lib/pio/I2S_Tx_naive.pio)
pico_generate_pio_header(I2S_Tx_test ${CMAKE_CURRENT_LIST_DIR}/src/lib/pio/I2S_Tx_compact.pio)

pico_enable_stdio_uart(I2S_Tx_test 0)
pico_enable_stdio_usb(I2S_Tx_test 1)

target_link_libraries(I2S_Tx_test
    pico_stdlib
    hardware_pio
    hardware_dma
)
target_include_directories(I2S_Tx_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)
pico_add_extra_outputs(I2S_Tx_test)

add_executable(I2S_Rx_test
    src/test/I2S_Rx_test.cpp
    src/lib/I2S.cpp
    src/lib/RxPingPong.cpp
)
pico_generate_pio_header(I2S_Rx_test ${CMAKE_CURRENT_LIST_DIR}/src/lib/pio/I2S_Rx_naive.pio)
pico_generate_pio_header(I2S_Rx_test ${CMAKE_CURRENT_LIST_DIR}/src/lib/pio/I2S_Rx_autoFrame.pio)

pico_enable_stdio_uart(I2S_Rx_test 0)
pico_enable_stdio_usb(I2S_Rx_test 1)

target_link_libraries(I2S_Rx_test
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_irq
)
target_include_directories(I2S_Rx_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)
pico_add_extra_outputs(I2S_Rx_test)

########## ROTARY ENCODER ##########
add_executable(RotaryEncoder_test
    src/test/RotaryEncoder_test.cpp
)

pico_enable_stdio_uart(RotaryEncoder_test 0)
pico_enable_stdio_usb(RotaryEncoder_test 1)

target_link_libraries(RotaryEncoder_test
    pico_stdlib
)
target_include_directories(RotaryEncoder_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)
pico_add_extra_outputs(RotaryEncoder_test)

########## PUSH BUTTON ##########
add_executable(PushButton_test
    src/test/PushButton_test.cpp
)

pico_enable_stdio_uart(PushButton_test 0)
pico_enable_stdio_usb(PushButton_test 1)

target_link_libraries(PushButton_test
    pico_stdlib
)
target_include_directories(PushButton_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)
pico_add_extra_outputs(PushButton_test)

endif()
