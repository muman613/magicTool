cmake_minimum_required(VERSION 3.16)

project(debug_tool_qt5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DEBUG_TOOL_QT5_BUILD_EXAMPLES "Build debug_tool_qt5 example programs" OFF)

find_package(Qt5 5.12 REQUIRED COMPONENTS Core SerialPort)

add_library(debug_tool_qt5 STATIC
    src/DebugToolDevice.cpp
)

add_library(debug_tool::qt5 ALIAS debug_tool_qt5)

target_include_directories(debug_tool_qt5
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(debug_tool_qt5
    PUBLIC
        Qt5::Core
        Qt5::SerialPort
)

set_target_properties(debug_tool_qt5 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

if(DEBUG_TOOL_QT5_BUILD_EXAMPLES)
    add_executable(debug_tool_qt5_basic_example
        examples/basic_usage.cpp
    )

    target_link_libraries(debug_tool_qt5_basic_example
        PRIVATE
            debug_tool_qt5
    )
endif()
