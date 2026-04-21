cmake_minimum_required(VERSION 3.16)

project(magicTool VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/opt/magictool" CACHE PATH "Install path prefix" FORCE)
endif()

set(MAGICTOOL_INSTALL_BINDIR "bin" CACHE STRING "Executable install directory")
set(MAGICTOOL_INSTALL_LIBDIR "lib" CACHE STRING "Library install directory")
set(MAGICTOOL_INSTALL_INCLUDEDIR "inc" CACHE STRING "Header install directory")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DEBUG_TOOL_BUILD_QT5 "Build the Qt5 magictool host library" ON)
option(DEBUG_TOOL_BUILD_NATIVE "Build the native magictool host library" ON)
option(DEBUG_TOOL_QT5_BUILD_EXAMPLES "Build magictool example programs" OFF)
set(DEBUG_TOOL_NATIVE_BUILD_EXAMPLES ${DEBUG_TOOL_QT5_BUILD_EXAMPLES} CACHE BOOL "Build native magictool example programs")

set(MAGICTOOL_HOST_EXAMPLE_VERSION_MAJOR "0" CACHE STRING "Host example major version")
set(MAGICTOOL_HOST_EXAMPLE_VERSION_MINOR "1" CACHE STRING "Host example minor version")
set(MAGICTOOL_HOST_EXAMPLE_VERSION_REVISION "0" CACHE STRING "Host example revision version")

set(MAGICTOOL_LIBRARY_VERSION_DEFINES
    MAGICTOOL_LIBRARY_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    MAGICTOOL_LIBRARY_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    MAGICTOOL_LIBRARY_VERSION_REVISION=${PROJECT_VERSION_PATCH}
)

set(MAGICTOOL_HOST_EXAMPLE_VERSION_DEFINES
    MAGICTOOL_HOST_EXAMPLE_VERSION_MAJOR=${MAGICTOOL_HOST_EXAMPLE_VERSION_MAJOR}
    MAGICTOOL_HOST_EXAMPLE_VERSION_MINOR=${MAGICTOOL_HOST_EXAMPLE_VERSION_MINOR}
    MAGICTOOL_HOST_EXAMPLE_VERSION_REVISION=${MAGICTOOL_HOST_EXAMPLE_VERSION_REVISION}
)

if(DEBUG_TOOL_BUILD_NATIVE)
    add_library(magictool_native_lib STATIC
        src/magicdebug_native.cpp
    )

    add_library(magicTool::native ALIAS magictool_native_lib)

    set_target_properties(magictool_native_lib PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME magictool_native
    )

    target_include_directories(magictool_native_lib
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${MAGICTOOL_INSTALL_INCLUDEDIR}>
    )

    target_compile_definitions(magictool_native_lib
        PUBLIC
            ${MAGICTOOL_LIBRARY_VERSION_DEFINES}
    )

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/magictool_native.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/magictool_native.pc
        @ONLY
    )

    install(TARGETS magictool_native_lib
        ARCHIVE DESTINATION ${MAGICTOOL_INSTALL_LIBDIR}
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/magictool_native.pc
        DESTINATION ${MAGICTOOL_INSTALL_LIBDIR}/pkgconfig
    )
endif()

if(DEBUG_TOOL_BUILD_QT5)
    find_package(Qt5 5.12 REQUIRED COMPONENTS Core SerialPort)

    add_library(magictool_qt5_lib STATIC
        src/magicdebug.cpp
    )

    add_library(magicTool::qt5 ALIAS magictool_qt5_lib)

    target_link_libraries(magictool_qt5_lib
        PUBLIC
            Qt5::Core
            Qt5::SerialPort
    )

    set_target_properties(magictool_qt5_lib PROPERTIES
        AUTOMOC ON
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME magictool_qt5
    )

    target_include_directories(magictool_qt5_lib
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${MAGICTOOL_INSTALL_INCLUDEDIR}>
    )

    target_compile_definitions(magictool_qt5_lib
        PUBLIC
            ${MAGICTOOL_LIBRARY_VERSION_DEFINES}
    )

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/magictool.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/magictool_qt5.pc
        @ONLY
    )

    install(TARGETS magictool_qt5_lib
        ARCHIVE DESTINATION ${MAGICTOOL_INSTALL_LIBDIR}
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/magictool_qt5.pc
        DESTINATION ${MAGICTOOL_INSTALL_LIBDIR}/pkgconfig
    )

    if(DEBUG_TOOL_QT5_BUILD_EXAMPLES)
        find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets)

        add_executable(magictool_example
            examples/basic_usage.cpp
        )

        target_link_libraries(magictool_example
            PRIVATE
                magictool_qt5_lib
        )

        target_compile_definitions(magictool_example
            PRIVATE
                ${MAGICTOOL_HOST_EXAMPLE_VERSION_DEFINES}
        )

        set_target_properties(magictool_example PROPERTIES
            AUTOMOC ON
            OUTPUT_NAME magictool
        )

        add_executable(magicUI
            examples/magicUI.cpp
        )

        target_link_libraries(magicUI
            PRIVATE
                magictool_qt5_lib
                Qt5::Widgets
        )

        target_compile_definitions(magicUI
            PRIVATE
                ${MAGICTOOL_HOST_EXAMPLE_VERSION_DEFINES}
        )

        set_target_properties(magicUI PROPERTIES
            AUTOMOC ON
        )

        install(TARGETS magictool_example magicUI
            RUNTIME DESTINATION ${MAGICTOOL_INSTALL_BINDIR}
        )
    endif()
endif()

if(DEBUG_TOOL_BUILD_NATIVE AND DEBUG_TOOL_NATIVE_BUILD_EXAMPLES)
    add_executable(magictool_native
        examples/basic_usage_native.cpp
    )

    target_link_libraries(magictool_native
        PRIVATE
            magictool_native_lib
    )

    target_compile_definitions(magictool_native
        PRIVATE
            ${MAGICTOOL_HOST_EXAMPLE_VERSION_DEFINES}
    )

    install(TARGETS magictool_native
        RUNTIME DESTINATION ${MAGICTOOL_INSTALL_BINDIR}
    )
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${MAGICTOOL_INSTALL_INCLUDEDIR}
)
