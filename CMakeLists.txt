cmake_minimum_required(VERSION 3.16)

project(magicTool LANGUAGES NONE)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/opt/magictool" CACHE PATH "Install path prefix" FORCE)
endif()

include(ExternalProject)

option(DEBUG_TOOL_BUILD_FIRMWARE "Build the Pico firmware" ON)
option(DEBUG_TOOL_BUILD_HOST "Build the host libraries and applications" ON)
option(DEBUG_TOOL_QT5_BUILD_EXAMPLES "Build magicTool Qt5 example programs" OFF)
set(DEBUG_TOOL_NATIVE_BUILD_EXAMPLES ${DEBUG_TOOL_QT5_BUILD_EXAMPLES} CACHE BOOL "Build magicTool native example programs")
option(PICO_2_W "Build firmware for Raspberry Pi Pico 2 W" OFF)
set(MAGICTOOL_HW_VERSION "1" CACHE STRING "Hardware revision nibble reported by GET_HARDWARE_VERSION")

set(DEBUG_TOOL_COMMON_CMAKE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
)

if(DEBUG_TOOL_BUILD_FIRMWARE)
    if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
    endif()

    if(NOT PICO_SDK_PATH)
        message(FATAL_ERROR "PICO_SDK_PATH must be set to build the firmware")
    endif()
endif()

if(DEBUG_TOOL_BUILD_FIRMWARE)
    ExternalProject_Add(magicTool_firmware
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/firmware
        BINARY_DIR ${CMAKE_BINARY_DIR}/firmware
        CMAKE_ARGS
            ${DEBUG_TOOL_COMMON_CMAKE_ARGS}
            -DPICO_SDK_PATH:PATH=${PICO_SDK_PATH}
            -DPICO_2_W:BOOL=${PICO_2_W}
            -DMAGICTOOL_HW_VERSION:STRING=${MAGICTOOL_HW_VERSION}
        INSTALL_COMMAND ""
        USES_TERMINAL_CONFIGURE TRUE
        USES_TERMINAL_BUILD TRUE
    )
endif()

if(DEBUG_TOOL_BUILD_HOST)
    if(DEBUG_TOOL_BUILD_FIRMWARE)
        ExternalProject_Add(magicTool_host
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/host
            BINARY_DIR ${CMAKE_BINARY_DIR}/host
            CMAKE_ARGS
                ${DEBUG_TOOL_COMMON_CMAKE_ARGS}
                -DDEBUG_TOOL_QT5_BUILD_EXAMPLES:BOOL=${DEBUG_TOOL_QT5_BUILD_EXAMPLES}
                -DDEBUG_TOOL_NATIVE_BUILD_EXAMPLES:BOOL=${DEBUG_TOOL_NATIVE_BUILD_EXAMPLES}
            INSTALL_COMMAND ""
            USES_TERMINAL_CONFIGURE TRUE
            USES_TERMINAL_BUILD TRUE
        )

        install(CODE [[
            execute_process(
                COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}/host"
                RESULT_VARIABLE magicTool_host_install_result
            )
            if(NOT magicTool_host_install_result EQUAL 0)
                message(FATAL_ERROR "Host install failed with exit code ${magicTool_host_install_result}")
            endif()
        ]])
    else()
        add_subdirectory(host)
    endif()
endif()
