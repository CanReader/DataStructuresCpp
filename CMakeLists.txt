cmake_minimum_required(VERSION 3.20)

project(dsc
    VERSION 1.0.0
    DESCRIPTION "Zero-dependency, high-performance C++ data structures and algorithms library"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only interface library
add_library(dsc INTERFACE)
add_library(dsc::dsc ALIAS dsc)

target_include_directories(dsc INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(dsc INTERFACE cxx_std_20)

# Examples
option(DSC_BUILD_EXAMPLES "Build dsc examples" ON)

if(DSC_BUILD_EXAMPLES)
    add_executable(dsc_example examples/basic_usage.cpp)
    target_link_libraries(dsc_example PRIVATE dsc::dsc)

    if(MSVC)
        target_compile_options(dsc_example PRIVATE /W4)
    else()
        target_compile_options(dsc_example PRIVATE -Wall -Wextra -Wpedantic -O2)
    endif()
endif()

# Install
include(GNUInstallDirs)
install(TARGETS dsc EXPORT dscTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT dscTargets
    FILE dscTargets.cmake
    NAMESPACE dsc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dsc
)
