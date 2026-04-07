cmake_minimum_required(VERSION 3.16)

project(magictool VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/opt/magictool" CACHE PATH "Install path prefix" FORCE)
endif()

set(MAGICTOOL_INSTALL_BINDIR "bin" CACHE STRING "Executable install directory")
set(MAGICTOOL_INSTALL_LIBDIR "lib" CACHE STRING "Library install directory")
set(MAGICTOOL_INSTALL_INCLUDEDIR "inc" CACHE STRING "Header install directory")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

option(DEBUG_TOOL_QT5_BUILD_EXAMPLES "Build magictool example programs" OFF)

find_package(Qt5 5.12 REQUIRED COMPONENTS Core SerialPort)

add_library(magictool_lib STATIC
    src/magicdebug.cpp
)

add_library(debug_tool::qt5 ALIAS magictool_lib)

target_link_libraries(magictool_lib
    PUBLIC
        Qt5::Core
        Qt5::SerialPort
)

set_target_properties(magictool_lib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME magictool
)

target_include_directories(magictool_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${MAGICTOOL_INSTALL_INCLUDEDIR}>
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/magictool.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/magictool.pc
    @ONLY
)

install(TARGETS magictool_lib
    ARCHIVE DESTINATION ${MAGICTOOL_INSTALL_LIBDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${MAGICTOOL_INSTALL_INCLUDEDIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/magictool.pc
    DESTINATION ${MAGICTOOL_INSTALL_LIBDIR}/pkgconfig
)

if(DEBUG_TOOL_QT5_BUILD_EXAMPLES)
    find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets)

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

    add_executable(magicUI
        examples/magicUI.cpp
    )

    target_link_libraries(magicUI
        PRIVATE
            magictool_lib
            Qt5::Widgets
    )

    install(TARGETS magictool_example magicUI
        RUNTIME DESTINATION ${MAGICTOOL_INSTALL_BINDIR}
    )
endif()
