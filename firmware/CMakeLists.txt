cmake_minimum_required(VERSION 3.13)

if(NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()

if(NOT PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH must be set to build the firmware")
endif()

set(PICO_BOARD pico2_w)

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_debug_gpio LANGUAGES C CXX ASM)

pico_sdk_init()

add_executable(pico_debug_gpio
    main.cpp
    usb_descriptors.c
)

target_include_directories(pico_debug_gpio
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(pico_debug_gpio
    pico_stdlib
    pico_multicore
    tinyusb_device
    tinyusb_board
)

pico_enable_stdio_usb(pico_debug_gpio 0)
pico_enable_stdio_uart(pico_debug_gpio 0)

pico_add_extra_outputs(pico_debug_gpio)
