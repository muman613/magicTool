cmake_minimum_required(VERSION 3.13)

if(NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()

if(NOT PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH must be set to build the firmware")
endif()

option(PICO_2_W "Build for Raspberry Pi Pico 2 W" OFF)
set(MAGICTOOL_HW_VERSION "1" CACHE STRING "Hardware revision nibble reported by GET_HARDWARE_VERSION")
set(MAGICTOOL_FW_VERSION_MAJOR "0" CACHE STRING "Firmware major version")
set(MAGICTOOL_FW_VERSION_MINOR "1" CACHE STRING "Firmware minor version")
set(MAGICTOOL_FW_VERSION_REVISION "0" CACHE STRING "Firmware revision version")

if(PICO_2_W)
    set(PICO_BOARD pico2_w)
    set(PICO_DEBUG_TARGET_DEFINE PICO_DEBUG_TARGET_PICO2_W)
    set(MAGICTOOL_OUTPUT_NAME magictool_fw_pico2_w)
else()
    set(PICO_BOARD pico2)
    set(PICO_DEBUG_TARGET_DEFINE PICO_DEBUG_TARGET_PICO2)
    set(MAGICTOOL_OUTPUT_NAME magictool_fw_pico2)
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(magictool_fw LANGUAGES C CXX ASM)

pico_sdk_init()

add_executable(magictool_fw
    main.cpp
    reset_interface.c
    usb_descriptors.c
)

set_target_properties(magictool_fw PROPERTIES
    OUTPUT_NAME ${MAGICTOOL_OUTPUT_NAME}
)

pico_set_program_name(magictool_fw "magictool_fw")
pico_set_program_description(magictool_fw "USB CDC GPIO debug firmware. Outputs: GPIO 2-5. Inputs: GPIO 6-7 with pulldowns. OPEN/CLOSE controls the onboard indicator LED.")

target_include_directories(magictool_fw
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_definitions(magictool_fw
    PRIVATE
        ${PICO_DEBUG_TARGET_DEFINE}=1
        MAGICTOOL_HW_VERSION=${MAGICTOOL_HW_VERSION}
        MAGICTOOL_FW_VERSION_MAJOR=${MAGICTOOL_FW_VERSION_MAJOR}
        MAGICTOOL_FW_VERSION_MINOR=${MAGICTOOL_FW_VERSION_MINOR}
        MAGICTOOL_FW_VERSION_REVISION=${MAGICTOOL_FW_VERSION_REVISION}
)

target_link_libraries(magictool_fw
    hardware_watchdog
    pico_bootrom
    pico_stdlib
    pico_multicore
    pico_usb_reset_interface_headers
    tinyusb_device
    tinyusb_board
)

if(PICO_2_W)
    target_link_libraries(magictool_fw
        pico_cyw43_arch_none
    )
endif()

pico_enable_stdio_usb(magictool_fw 0)
pico_enable_stdio_uart(magictool_fw 0)

pico_add_extra_outputs(magictool_fw)
