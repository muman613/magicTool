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
set(MAGICTOOL_FW_VERSION_MINOR "2" CACHE STRING "Firmware minor version")
set(MAGICTOOL_FW_VERSION_REVISION "0" CACHE STRING "Firmware revision version")
if(MAGICTOOL_HW_VERSION GREATER 1)
    set(MAGICTOOL_ENABLE_DISPLAY ON)
else()
    set(MAGICTOOL_ENABLE_DISPLAY OFF)
endif()

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
include(FetchContent)

project(magictool_fw LANGUAGES C CXX ASM)

pico_sdk_init()

option(MAGICTOOL_FETCH_FREERTOS "Fetch Raspberry Pi FreeRTOS-Kernel when FREERTOS_KERNEL_PATH is not set" ON)
set(FREERTOS_KERNEL_GIT_TAG "4f7299d6ea746b27a9dd19e87af568e34bd65b15" CACHE STRING "Raspberry Pi FreeRTOS-Kernel git tag, branch, or commit to fetch")
set(_magict_freertos_fetch_enabled "${MAGICTOOL_FETCH_FREERTOS}")
set(_magict_freertos_git_tag "${FREERTOS_KERNEL_GIT_TAG}")

if(NOT FREERTOS_KERNEL_PATH)
    if(DEFINED ENV{FREERTOS_KERNEL_PATH})
        set(FREERTOS_KERNEL_PATH $ENV{FREERTOS_KERNEL_PATH})
    elseif(EXISTS "/home/michael/gitroot/pico-dev/FreeRTOS-Kernel")
        set(FREERTOS_KERNEL_PATH "/home/michael/gitroot/pico-dev/FreeRTOS-Kernel")
    elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/external/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ")
        set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_LIST_DIR}/external/FreeRTOS-Kernel")
    elseif(_magict_freertos_fetch_enabled)
        message(STATUS "Fetching Raspberry Pi FreeRTOS-Kernel ${_magict_freertos_git_tag}")
        FetchContent_Declare(
            freertos_kernel_fetched
            GIT_REPOSITORY https://github.com/raspberrypi/FreeRTOS-Kernel.git
            GIT_TAG ${_magict_freertos_git_tag}
            GIT_SHALLOW FALSE
        )
        FetchContent_Populate(freertos_kernel_fetched)
        set(FREERTOS_KERNEL_PATH "${freertos_kernel_fetched_SOURCE_DIR}")
    else()
        message(FATAL_ERROR
            "Set FREERTOS_KERNEL_PATH to your Raspberry Pi FreeRTOS-Kernel checkout "
            "or enable -DMAGICTOOL_FETCH_FREERTOS=ON."
        )
    endif()
endif()

if(NOT EXISTS "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/CMakeLists.txt")
    message(FATAL_ERROR
        "FreeRTOS-Kernel at '${FREERTOS_KERNEL_PATH}' does not contain the RP2350 ARM non-secure port. "
        "Use the Raspberry Pi FreeRTOS-Kernel fork or enable -DMAGICTOOL_FETCH_FREERTOS=ON."
    )
endif()

set(FREERTOS_CONFIG_FILE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include)
add_subdirectory(
    ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ
    ${CMAKE_BINARY_DIR}/FreeRTOS-Kernel
)

if(MAGICTOOL_ENABLE_DISPLAY)
    if(NOT LVGL_PATH)
        if(DEFINED ENV{LVGL_PATH})
            set(LVGL_PATH $ENV{LVGL_PATH})
        elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/external/lvgl/CMakeLists.txt")
            set(LVGL_PATH "${CMAKE_CURRENT_LIST_DIR}/external/lvgl")
        endif()
    endif()
endif()

option(MAGICTOOL_FETCH_LVGL "Fetch LVGL from GitHub when LVGL_PATH is not set" ON)
set(LVGL_GIT_TAG "latest" CACHE STRING "LVGL git tag to fetch, or 'latest' for the newest stable vX.Y.Z tag")
set(MAGICTOOL_LVGL_FETCHED OFF)

if(MAGICTOOL_ENABLE_DISPLAY AND NOT LVGL_PATH AND MAGICTOOL_FETCH_LVGL)
    set(_lvgl_resolved_tag "${LVGL_GIT_TAG}")

    if(_lvgl_resolved_tag STREQUAL "latest")
        find_package(Git REQUIRED)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} ls-remote --tags --refs https://github.com/lvgl/lvgl.git "refs/tags/v[0-9]*"
            OUTPUT_VARIABLE _lvgl_remote_tags
            RESULT_VARIABLE _lvgl_remote_result
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(NOT _lvgl_remote_result EQUAL 0)
            message(FATAL_ERROR
                "Unable to query LVGL releases from GitHub. "
                "Set LVGL_PATH to a local checkout, set LVGL_GIT_TAG to a specific tag, "
                "or disable with -DMAGICTOOL_FETCH_LVGL=OFF."
            )
        endif()

        string(REGEX MATCHALL "refs/tags/v[0-9]+\\.[0-9]+\\.[0-9]+" _lvgl_tag_refs "${_lvgl_remote_tags}")
        set(_lvgl_stable_tags)
        foreach(_lvgl_tag_ref IN LISTS _lvgl_tag_refs)
            string(REPLACE "refs/tags/" "" _lvgl_tag "${_lvgl_tag_ref}")
            list(APPEND _lvgl_stable_tags "${_lvgl_tag}")
        endforeach()

        list(REMOVE_DUPLICATES _lvgl_stable_tags)
        list(SORT _lvgl_stable_tags COMPARE NATURAL ORDER DESCENDING)
        list(GET _lvgl_stable_tags 0 _lvgl_resolved_tag)
    endif()

    message(STATUS "Fetching LVGL ${_lvgl_resolved_tag}")
    FetchContent_Declare(
        lvgl_fetched
        URL https://github.com/lvgl/lvgl/archive/refs/tags/${_lvgl_resolved_tag}.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(MAGICTOOL_LVGL_FETCHED ON)
endif()

set(MAGICTOOL_ENABLE_LVGL OFF CACHE BOOL "Build LVGL UI support")
if(MAGICTOOL_ENABLE_DISPLAY AND (LVGL_PATH OR MAGICTOOL_LVGL_FETCHED))
    set(MAGICTOOL_ENABLE_LVGL ON CACHE BOOL "Build LVGL UI support" FORCE)
endif()

if(MAGICTOOL_ENABLE_DISPLAY AND MAGICTOOL_ENABLE_LVGL)
    set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
    set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LV_BUILD_CONF_DIR ${CMAKE_CURRENT_LIST_DIR}/include CACHE PATH "" FORCE)
    set(LV_BUILD_SET_CONFIG_OPTS ON CACHE BOOL "" FORCE)
    set(LV_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/include/lv_conf.h CACHE PATH "" FORCE)
    set(LV_CONF_INCLUDE_SIMPLE ON CACHE BOOL "" FORCE)

    if(MAGICTOOL_LVGL_FETCHED)
        FetchContent_MakeAvailable(lvgl_fetched)
    else()
        add_subdirectory(${LVGL_PATH} ${CMAKE_BINARY_DIR}/lvgl EXCLUDE_FROM_ALL)
    endif()
endif()

set(MAGICTOOL_FW_SOURCES
    main.cpp
    reset_interface.c
    usb_descriptors.c
)

if(MAGICTOOL_ENABLE_DISPLAY)
    list(APPEND MAGICTOOL_FW_SOURCES
        src/display/st7735.c
        src/ui/ui_lvgl.c
    )
endif()

add_executable(magictool_fw
    ${MAGICTOOL_FW_SOURCES}
)

set_target_properties(magictool_fw PROPERTIES
    OUTPUT_NAME ${MAGICTOOL_OUTPUT_NAME}
)

pico_set_program_name(magictool_fw "magictool_fw")
pico_set_program_description(magictool_fw "USB CDC GPIO debug firmware. Outputs: GPIO 2-5. Inputs: GPIO 6-7 with pulldowns. OPEN/CLOSE controls the onboard indicator LED.")

target_include_directories(magictool_fw
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_definitions(magictool_fw
    PRIVATE
        ${PICO_DEBUG_TARGET_DEFINE}=1
        MAGICTOOL_HW_VERSION=${MAGICTOOL_HW_VERSION}
        MAGICTOOL_FW_VERSION_MAJOR=${MAGICTOOL_FW_VERSION_MAJOR}
        MAGICTOOL_FW_VERSION_MINOR=${MAGICTOOL_FW_VERSION_MINOR}
        MAGICTOOL_FW_VERSION_REVISION=${MAGICTOOL_FW_VERSION_REVISION}
        MAGICTOOL_ENABLE_DISPLAY=$<BOOL:${MAGICTOOL_ENABLE_DISPLAY}>
        MAGICTOOL_ENABLE_LVGL=$<BOOL:${MAGICTOOL_ENABLE_LVGL}>
)

target_link_libraries(magictool_fw
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    hardware_watchdog
    pico_bootrom
    pico_stdlib
    pico_usb_reset_interface_headers
    tinyusb_device
    tinyusb_board
)

if(MAGICTOOL_ENABLE_DISPLAY)
    target_link_libraries(magictool_fw
        hardware_spi
    )
endif()

if(MAGICTOOL_ENABLE_DISPLAY AND MAGICTOOL_ENABLE_LVGL)
    target_link_libraries(magictool_fw
        lvgl
    )
endif()

if(PICO_2_W)
    target_link_libraries(magictool_fw
        pico_cyw43_arch_none
    )
endif()

pico_enable_stdio_usb(magictool_fw 0)
pico_enable_stdio_uart(magictool_fw 0)

pico_add_extra_outputs(magictool_fw)
