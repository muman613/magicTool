cmake_minimum_required(VERSION 3.16)

project(magictool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DEBUG_TOOL_QT5_BUILD_EXAMPLES "Build magictool example programs" OFF)

find_package(Qt5 5.12 REQUIRED COMPONENTS Core SerialPort)

add_library(magictool_lib STATIC
    src/DebugToolDevice.cpp
)

add_library(debug_tool::qt5 ALIAS magictool_lib)

target_include_directories(magictool_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(magictool_lib
    PUBLIC
        Qt5::Core
        Qt5::SerialPort
)

set_target_properties(magictool_lib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME magictool
)

if(DEBUG_TOOL_QT5_BUILD_EXAMPLES)
    add_executable(magictool_example
        examples/basic_usage.cpp
    )

    target_link_libraries(magictool_example
        PRIVATE
            magictool_lib
    )

    set_target_properties(magictool_example PROPERTIES
        OUTPUT_NAME magictool
    )
endif()
