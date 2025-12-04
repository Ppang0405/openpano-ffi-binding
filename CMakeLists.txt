# =============================================================================
# OpenPano FFI Portable Build System
# =============================================================================
# 
# This CMakeLists.txt builds the OpenPano library with FFI bindings for
# cross-platform desktop and mobile applications.
#
# Usage:
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# Options:
#   -DBUILD_SHARED_LIBS=ON/OFF   Build shared library (default: OFF)
#   -DBUILD_TESTS=ON/OFF         Build tests (default: OFF)
#   -DBUILD_EXAMPLES=ON/OFF      Build example programs (default: OFF)
#
# =============================================================================

cmake_minimum_required(VERSION 3.20)
project(OpenPanoFFI VERSION 1.0.0 LANGUAGES CXX C)

# =============================================================================
# Build Options
# =============================================================================

option(BUILD_SHARED_LIBS "Build shared libraries instead of static" OFF)
option(BUILD_TESTS "Build test programs" OFF)
option(BUILD_EXAMPLES "Build example programs" OFF)
option(OPENPANO_USE_OPENMP "Enable OpenMP for parallelization" ON)

# =============================================================================
# C++ Standard and Compiler Flags
# =============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Default to Release build
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Compiler-specific flags
if(MSVC)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
        WIN32_LEAN_AND_MEAN
        VC_EXTRALEAN
        MSVC
        UNICODE
        _UNICODE
    )
    add_compile_options(/fp:fast /GR- /O2 /Zo)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        add_compile_options(/arch:AVX)
    endif()
else()
    add_compile_options(-O3 -Wall -Wextra)
    # Only use -march=native when:
    # - Not cross-compiling
    # - Not iOS
    # - Not building for a specific architecture (like universal binaries)
    # - Not Android
    if(NOT CMAKE_CROSSCOMPILING 
       AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS"
       AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android"
       AND NOT CMAKE_OSX_ARCHITECTURES)
        add_compile_options(-march=native)
    endif()
endif()

# Visibility for shared libraries
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# CImg compatibility for mobile platforms
# Disable system calls and display on iOS/Android (not available on sandboxed platforms)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_compile_definitions(
        OPENPANO_MOBILE_PLATFORM=1   # Signals to use lodepng only
        cimg_no_system_calls         # Disable std::system() - not available on iOS/Android
        cimg_display=0               # Disable display code - not needed for headless processing
    )
endif()

# =============================================================================
# Find Dependencies
# =============================================================================

# OpenMP (optional but recommended)
if(OPENPANO_USE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_FOUND)
        message(STATUS "OpenMP found - parallel processing enabled")
    else()
        message(WARNING "OpenMP not found - will be single-threaded")
    endif()
endif()

# Eigen3 (required)
find_package(Eigen3 REQUIRED)
message(STATUS "Eigen3 found: ${EIGEN3_INCLUDE_DIRS}")

# JPEG (optional)
find_package(JPEG)
if(JPEG_FOUND)
    message(STATUS "JPEG support enabled")
else()
    message(STATUS "JPEG support disabled - only PNG will be available")
    add_compile_definitions(DISABLE_JPEG)
endif()

# =============================================================================
# Source Files
# =============================================================================

set(OPENPANO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenPano/src)

# Core OpenPano sources
file(GLOB OPENPANO_FEATURE_SOURCES ${OPENPANO_DIR}/feature/*.cc)
file(GLOB OPENPANO_STITCH_SOURCES ${OPENPANO_DIR}/stitch/*.cc)
file(GLOB OPENPANO_LIB_SOURCES ${OPENPANO_DIR}/lib/*.cc)

set(OPENPANO_SOURCES
    ${OPENPANO_FEATURE_SOURCES}
    ${OPENPANO_STITCH_SOURCES}
    ${OPENPANO_LIB_SOURCES}
)

# Third-party sources (lodepng)
set(LODEPNG_SOURCES
    ${OPENPANO_DIR}/third-party/lodepng/lodepng.cc
)

# FFI sources
set(FFI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ffi/src/openpano_ffi.cpp
)

# =============================================================================
# OpenPano Core Library
# =============================================================================

add_library(openpano_core STATIC ${OPENPANO_SOURCES})

target_include_directories(openpano_core PUBLIC
    $<BUILD_INTERFACE:${OPENPANO_DIR}>
    $<BUILD_INTERFACE:${OPENPANO_DIR}/third-party>
    $<INSTALL_INTERFACE:include/openpano>
)

target_link_libraries(openpano_core PUBLIC Eigen3::Eigen)

if(OpenMP_FOUND)
    target_link_libraries(openpano_core PUBLIC OpenMP::OpenMP_CXX)
endif()

if(JPEG_FOUND)
    target_include_directories(openpano_core PRIVATE ${JPEG_INCLUDE_DIRS})
    target_link_libraries(openpano_core PRIVATE ${JPEG_LIBRARIES})
else()
    target_compile_definitions(openpano_core PUBLIC DISABLE_JPEG)
endif()

# =============================================================================
# LodePNG Library (always included for PNG support)
# =============================================================================

add_library(lodepng STATIC ${LODEPNG_SOURCES})
target_include_directories(lodepng PUBLIC
    $<BUILD_INTERFACE:${OPENPANO_DIR}/third-party/lodepng>
)

# =============================================================================
# OpenPano FFI Library (the main distributable)
# =============================================================================

if(BUILD_SHARED_LIBS)
    add_library(openpano_ffi SHARED ${FFI_SOURCES})
    target_compile_definitions(openpano_ffi PRIVATE OPENPANO_EXPORTS)
else()
    add_library(openpano_ffi STATIC ${FFI_SOURCES})
endif()

# Create alias for consistent naming
add_library(OpenPano::FFI ALIAS openpano_ffi)

target_include_directories(openpano_ffi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ffi/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(openpano_ffi PRIVATE openpano_core lodepng)

# Set output name
set_target_properties(openpano_ffi PROPERTIES
    OUTPUT_NAME "openpano"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# =============================================================================
# Combined Static Library (for easy distribution)
# =============================================================================

# Create a "fat" static library combining all dependencies
# This makes it easier to distribute a single .a file
if(NOT BUILD_SHARED_LIBS)
    # For static builds, we create an archive that includes all objects
    add_library(openpano_static STATIC ${FFI_SOURCES} ${OPENPANO_SOURCES} ${LODEPNG_SOURCES})
    
    target_include_directories(openpano_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ffi/include>
        $<BUILD_INTERFACE:${OPENPANO_DIR}>
        $<BUILD_INTERFACE:${OPENPANO_DIR}/third-party>
        $<BUILD_INTERFACE:${OPENPANO_DIR}/third-party/lodepng>
        $<INSTALL_INTERFACE:include>
    )
    
    target_link_libraries(openpano_static PUBLIC Eigen3::Eigen)
    
    if(OpenMP_FOUND)
        target_link_libraries(openpano_static PUBLIC OpenMP::OpenMP_CXX)
    endif()
    
    if(JPEG_FOUND)
        target_include_directories(openpano_static PRIVATE ${JPEG_INCLUDE_DIRS})
        target_link_libraries(openpano_static PRIVATE ${JPEG_LIBRARIES})
    else()
        target_compile_definitions(openpano_static PUBLIC DISABLE_JPEG)
    endif()
    
    set_target_properties(openpano_static PROPERTIES
        OUTPUT_NAME "openpano_all"
    )
endif()

# =============================================================================
# Example Programs
# =============================================================================

if(BUILD_EXAMPLES)
    add_executable(pano_example examples/pano_example.c)
    target_link_libraries(pano_example PRIVATE openpano_ffi)
    
    # Need to link C++ runtime for the C example
    if(NOT MSVC)
        target_link_libraries(pano_example PRIVATE stdc++)
    endif()
endif()

# =============================================================================
# Installation
# =============================================================================

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install headers
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/ffi/include/openpano_ffi.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install libraries
install(TARGETS openpano_ffi openpano_core lodepng
    EXPORT OpenPanoTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(NOT BUILD_SHARED_LIBS)
    install(TARGETS openpano_static
        EXPORT OpenPanoTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

# Generate and install config files
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/OpenPanoConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/OpenPanoConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/OpenPanoConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OpenPano
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/OpenPanoConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/OpenPanoConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OpenPano
)

install(EXPORT OpenPanoTargets
    FILE OpenPanoTargets.cmake
    NAMESPACE OpenPano::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OpenPano
)

# =============================================================================
# Summary
# =============================================================================

message(STATUS "")
message(STATUS "OpenPano FFI Configuration Summary:")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared libs:    ${BUILD_SHARED_LIBS}")
message(STATUS "  OpenMP:         ${OpenMP_FOUND}")
message(STATUS "  JPEG support:   ${JPEG_FOUND}")
message(STATUS "  PNG support:    ON (lodepng)")
message(STATUS "")

