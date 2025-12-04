# OpenPano FFI Portable Architecture Guide

> A comprehensive guide to wrapping OpenPano C++ library with FFI bindings for cross-platform desktop and mobile applications.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Building OpenPano as a Library](#building-openpano-as-a-library)
3. [Creating C-Compatible Bindings](#creating-c-compatible-bindings)
4. [Platform-Specific Integration](#platform-specific-integration)
5. [Rust FFI Integration](#rust-ffi-integration)
6. [Go FFI Integration (cgo)](#go-ffi-integration-cgo)
7. [Mobile Integration](#mobile-integration)
8. [Cross-Platform Build System](#cross-platform-build-system)
9. [Recommended Tech Stacks](#recommended-tech-stacks)

---

## Architecture Overview

### Why FFI?

Foreign Function Interface (FFI) allows different programming languages to call functions written in another language. For OpenPano:

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│         (Rust/Go/Swift/Kotlin - Platform UI)                │
├─────────────────────────────────────────────────────────────┤
│                    FFI Binding Layer                         │
│              (C-compatible function exports)                 │
├─────────────────────────────────────────────────────────────┤
│                    OpenPano Core (C++)                       │
│         (Compiled as static/shared library)                  │
├─────────────────────────────────────────────────────────────┤
│                    Native Dependencies                       │
│              (Eigen, libjpeg, libpng, etc.)                 │
└─────────────────────────────────────────────────────────────┘
```

### Key Principles

1. **Single Source of Truth**: Core stitching logic lives in C++ (OpenPano)
2. **C ABI Compatibility**: Export functions using `extern "C"` for maximum portability
3. **Opaque Pointers**: Use handles/pointers to hide C++ implementation details
4. **Memory Safety**: Clear ownership rules for allocated resources
5. **Thread Safety**: Document and enforce thread safety requirements

---

## Building OpenPano as a Library

### Step 1: Clone OpenPano

```bash
git clone https://github.com/ppwwyyxx/OpenPano.git
cd OpenPano
```

### Step 2: Modify CMakeLists.txt for Library Output

Create or modify `CMakeLists.txt` to build as a library:

```cmake
cmake_minimum_required(VERSION 3.20)
project(OpenPano VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find dependencies
find_package(Eigen3 REQUIRED)
find_package(JPEG)
find_package(PNG)

# Collect source files (adjust paths as needed)
file(GLOB_RECURSE OPENPANO_SOURCES 
    "src/lib/*.cc"
    "src/lib/*.cpp"
)

# Exclude main.cc from library
list(FILTER OPENPANO_SOURCES EXCLUDE REGEX ".*main\\.cc$")

# Build as STATIC library (recommended for mobile)
add_library(openpano_static STATIC ${OPENPANO_SOURCES})

# Build as SHARED library (optional, for desktop)
add_library(openpano_shared SHARED ${OPENPANO_SOURCES})

# Set library properties
foreach(target openpano_static openpano_shared)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lib>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(${target} PUBLIC Eigen3::Eigen)
    
    if(JPEG_FOUND)
        target_link_libraries(${target} PRIVATE ${JPEG_LIBRARIES})
        target_compile_definitions(${target} PRIVATE HAS_JPEG)
    endif()
    
    if(PNG_FOUND)
        target_link_libraries(${target} PRIVATE PNG::PNG)
        target_compile_definitions(${target} PRIVATE HAS_PNG)
    endif()
endforeach()

# Set output names
set_target_properties(openpano_static PROPERTIES OUTPUT_NAME "openpano")
set_target_properties(openpano_shared PROPERTIES OUTPUT_NAME "openpano")

# Install targets
install(TARGETS openpano_static openpano_shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
```

### Step 3: Build Commands

```bash
# Desktop (macOS/Linux)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# iOS (requires Xcode)
cmake -B build-ios \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES="arm64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-ios

# Android (requires NDK)
cmake -B build-android \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-android
```

---

## Creating C-Compatible Bindings

### Header File: `openpano_ffi.h`

```c
/**
 * @file openpano_ffi.h
 * @brief C-compatible FFI bindings for OpenPano panorama stitching library
 * 
 * This header provides a portable C interface that can be called from
 * Rust, Go, Swift, Kotlin, and other languages via FFI.
 */

#ifndef OPENPANO_FFI_H
#define OPENPANO_FFI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Platform-specific export macros
 * ============================================================================ */

#if defined(_WIN32) || defined(_WIN64)
    #ifdef OPENPANO_EXPORTS
        #define OPENPANO_API __declspec(dllexport)
    #else
        #define OPENPANO_API __declspec(dllimport)
    #endif
#else
    #define OPENPANO_API __attribute__((visibility("default")))
#endif

/* ============================================================================
 * Opaque handle types
 * ============================================================================ */

/** Opaque handle to a stitcher instance */
typedef struct OpenpanoStitcher* OpenpanoStitcherHandle;

/** Opaque handle to a result image */
typedef struct OpenpanoImage* OpenpanoImageHandle;

/* ============================================================================
 * Error handling
 * ============================================================================ */

/** Error codes returned by OpenPano functions */
typedef enum OpenpanoError {
    OPENPANO_OK = 0,
    OPENPANO_ERROR_INVALID_HANDLE = -1,
    OPENPANO_ERROR_INVALID_ARGUMENT = -2,
    OPENPANO_ERROR_FILE_NOT_FOUND = -3,
    OPENPANO_ERROR_INSUFFICIENT_IMAGES = -4,
    OPENPANO_ERROR_STITCHING_FAILED = -5,
    OPENPANO_ERROR_OUT_OF_MEMORY = -6,
    OPENPANO_ERROR_FEATURE_DETECTION_FAILED = -7,
    OPENPANO_ERROR_MATCHING_FAILED = -8,
    OPENPANO_ERROR_UNKNOWN = -99
} OpenpanoError;

/** 
 * Get human-readable error message for an error code
 * @param error The error code
 * @return Static string describing the error (do not free)
 */
OPENPANO_API const char* openpano_error_string(OpenpanoError error);

/* ============================================================================
 * Stitching mode configuration
 * ============================================================================ */

/** Stitching modes supported by OpenPano */
typedef enum OpenpanoMode {
    /** Cylinder projection - camera rotates around vertical axis */
    OPENPANO_MODE_CYLINDER = 0,
    
    /** Camera estimation - bundle adjustment for arbitrary camera poses */
    OPENPANO_MODE_ESTIMATE_CAMERA = 1,
    
    /** Translation mode - pure translation between images */
    OPENPANO_MODE_TRANSLATION = 2
} OpenpanoMode;

/** Configuration options for stitching */
typedef struct OpenpanoConfig {
    /** Stitching mode */
    OpenpanoMode mode;
    
    /** Focal length in 35mm equivalent (used in CYLINDER mode) */
    float focal_length;
    
    /** Whether input images are in sequential order */
    int ordered_input;
    
    /** Whether to crop result to remove black borders */
    int crop_result;
    
    /** Number of threads to use (0 = auto) */
    int num_threads;
    
    /** Enable lazy image loading to reduce memory */
    int lazy_read;
    
    /** RANSAC confidence threshold (0.0 - 1.0) */
    float ransac_confidence;
    
    /** Minimum number of feature matches required */
    int min_matches;
} OpenpanoConfig;

/**
 * Get default configuration values
 * @param config Pointer to config struct to fill with defaults
 */
OPENPANO_API void openpano_config_default(OpenpanoConfig* config);

/* ============================================================================
 * Stitcher lifecycle
 * ============================================================================ */

/**
 * Create a new stitcher instance
 * @param config Configuration options (NULL for defaults)
 * @return Handle to stitcher, or NULL on failure
 */
OPENPANO_API OpenpanoStitcherHandle openpano_stitcher_create(
    const OpenpanoConfig* config
);

/**
 * Destroy a stitcher instance and free resources
 * @param handle Stitcher handle (safe to pass NULL)
 */
OPENPANO_API void openpano_stitcher_destroy(OpenpanoStitcherHandle handle);

/* ============================================================================
 * Image input methods
 * ============================================================================ */

/**
 * Add an image file to the stitcher
 * @param handle Stitcher handle
 * @param file_path Path to image file (UTF-8 encoded)
 * @return Error code
 */
OPENPANO_API OpenpanoError openpano_add_image_file(
    OpenpanoStitcherHandle handle,
    const char* file_path
);

/**
 * Add an image from raw pixel data
 * @param handle Stitcher handle
 * @param pixels Pointer to pixel data (RGB, 8-bit per channel)
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param stride Bytes per row (0 = width * 3)
 * @return Error code
 */
OPENPANO_API OpenpanoError openpano_add_image_data(
    OpenpanoStitcherHandle handle,
    const uint8_t* pixels,
    int32_t width,
    int32_t height,
    int32_t stride
);

/**
 * Clear all added images
 * @param handle Stitcher handle
 * @return Error code
 */
OPENPANO_API OpenpanoError openpano_clear_images(OpenpanoStitcherHandle handle);

/**
 * Get number of images currently added
 * @param handle Stitcher handle
 * @return Number of images, or -1 on error
 */
OPENPANO_API int32_t openpano_image_count(OpenpanoStitcherHandle handle);

/* ============================================================================
 * Stitching execution
 * ============================================================================ */

/** Progress callback function type */
typedef void (*OpenpanoProgressCallback)(
    float progress,      /* 0.0 to 1.0 */
    const char* stage,   /* Current stage description */
    void* user_data      /* User-provided context */
);

/**
 * Execute panorama stitching
 * @param handle Stitcher handle
 * @param result Output handle for result image
 * @return Error code
 */
OPENPANO_API OpenpanoError openpano_stitch(
    OpenpanoStitcherHandle handle,
    OpenpanoImageHandle* result
);

/**
 * Execute panorama stitching with progress callback
 * @param handle Stitcher handle
 * @param result Output handle for result image
 * @param callback Progress callback function
 * @param user_data Context passed to callback
 * @return Error code
 */
OPENPANO_API OpenpanoError openpano_stitch_with_progress(
    OpenpanoStitcherHandle handle,
    OpenpanoImageHandle* result,
    OpenpanoProgressCallback callback,
    void* user_data
);

/**
 * Cancel an ongoing stitching operation
 * @param handle Stitcher handle
 * @return Error code
 */
OPENPANO_API OpenpanoError openpano_cancel(OpenpanoStitcherHandle handle);

/* ============================================================================
 * Result image handling
 * ============================================================================ */

/**
 * Get result image dimensions
 * @param image Image handle
 * @param width Output width
 * @param height Output height
 * @return Error code
 */
OPENPANO_API OpenpanoError openpano_image_dimensions(
    OpenpanoImageHandle image,
    int32_t* width,
    int32_t* height
);

/**
 * Get pointer to result image pixel data
 * @param image Image handle
 * @param pixels Output pointer to pixel data (RGB, 8-bit)
 * @param size Output size of pixel data in bytes
 * @return Error code
 * @note Pointer is valid until image is destroyed
 */
OPENPANO_API OpenpanoError openpano_image_data(
    OpenpanoImageHandle image,
    const uint8_t** pixels,
    size_t* size
);

/**
 * Copy result image pixel data to provided buffer
 * @param image Image handle
 * @param buffer Destination buffer
 * @param buffer_size Size of destination buffer
 * @return Error code
 */
OPENPANO_API OpenpanoError openpano_image_copy_data(
    OpenpanoImageHandle image,
    uint8_t* buffer,
    size_t buffer_size
);

/**
 * Save result image to file
 * @param image Image handle
 * @param file_path Output file path (UTF-8)
 * @param quality JPEG quality 1-100 (ignored for PNG)
 * @return Error code
 */
OPENPANO_API OpenpanoError openpano_image_save(
    OpenpanoImageHandle image,
    const char* file_path,
    int quality
);

/**
 * Destroy result image and free memory
 * @param image Image handle (safe to pass NULL)
 */
OPENPANO_API void openpano_image_destroy(OpenpanoImageHandle image);

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/**
 * Get OpenPano library version string
 * @return Version string (e.g., "1.0.0")
 */
OPENPANO_API const char* openpano_version(void);

/**
 * Check if library was built with JPEG support
 * @return 1 if supported, 0 otherwise
 */
OPENPANO_API int openpano_has_jpeg_support(void);

/**
 * Check if library was built with PNG support
 * @return 1 if supported, 0 otherwise
 */
OPENPANO_API int openpano_has_png_support(void);

/**
 * Set global log level
 * @param level 0=none, 1=error, 2=warn, 3=info, 4=debug
 */
OPENPANO_API void openpano_set_log_level(int level);

/** Log callback function type */
typedef void (*OpenpanoLogCallback)(
    int level,
    const char* message,
    void* user_data
);

/**
 * Set custom log callback
 * @param callback Log callback function (NULL to disable)
 * @param user_data Context passed to callback
 */
OPENPANO_API void openpano_set_log_callback(
    OpenpanoLogCallback callback,
    void* user_data
);

#ifdef __cplusplus
}
#endif

#endif /* OPENPANO_FFI_H */
```

### Implementation File: `openpano_ffi.cpp`

```cpp
/**
 * @file openpano_ffi.cpp
 * @brief Implementation of C-compatible FFI bindings for OpenPano
 */

#include "openpano_ffi.h"

// Include OpenPano headers
#include "stitcher.hh"
#include "config.hh"
#include "imageref.hh"

#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

/* ============================================================================
 * Internal structures
 * ============================================================================ */

struct OpenpanoStitcher {
    std::vector<std::string> image_paths;
    std::vector<std::unique_ptr<pano::ImageRef>> image_data;
    OpenpanoConfig config;
    std::atomic<bool> cancelled{false};
    std::mutex mutex;
};

struct OpenpanoImage {
    std::vector<uint8_t> pixels;
    int32_t width;
    int32_t height;
};

/* ============================================================================
 * Error handling
 * ============================================================================ */

OPENPANO_API const char* openpano_error_string(OpenpanoError error) {
    switch (error) {
        case OPENPANO_OK: return "Success";
        case OPENPANO_ERROR_INVALID_HANDLE: return "Invalid handle";
        case OPENPANO_ERROR_INVALID_ARGUMENT: return "Invalid argument";
        case OPENPANO_ERROR_FILE_NOT_FOUND: return "File not found";
        case OPENPANO_ERROR_INSUFFICIENT_IMAGES: return "Insufficient images (need at least 2)";
        case OPENPANO_ERROR_STITCHING_FAILED: return "Stitching failed";
        case OPENPANO_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case OPENPANO_ERROR_FEATURE_DETECTION_FAILED: return "Feature detection failed";
        case OPENPANO_ERROR_MATCHING_FAILED: return "Feature matching failed";
        default: return "Unknown error";
    }
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

OPENPANO_API void openpano_config_default(OpenpanoConfig* config) {
    if (!config) return;
    
    config->mode = OPENPANO_MODE_ESTIMATE_CAMERA;
    config->focal_length = 36.0f;  // 35mm equivalent
    config->ordered_input = 0;
    config->crop_result = 1;
    config->num_threads = 0;  // auto
    config->lazy_read = 0;
    config->ransac_confidence = 0.9f;
    config->min_matches = 20;
}

/* ============================================================================
 * Stitcher lifecycle
 * ============================================================================ */

OPENPANO_API OpenpanoStitcherHandle openpano_stitcher_create(const OpenpanoConfig* config) {
    try {
        auto* stitcher = new OpenpanoStitcher();
        if (config) {
            stitcher->config = *config;
        } else {
            openpano_config_default(&stitcher->config);
        }
        return stitcher;
    } catch (...) {
        return nullptr;
    }
}

OPENPANO_API void openpano_stitcher_destroy(OpenpanoStitcherHandle handle) {
    delete handle;
}

/* ============================================================================
 * Image input
 * ============================================================================ */

OPENPANO_API OpenpanoError openpano_add_image_file(
    OpenpanoStitcherHandle handle,
    const char* file_path
) {
    if (!handle) return OPENPANO_ERROR_INVALID_HANDLE;
    if (!file_path) return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    std::lock_guard<std::mutex> lock(handle->mutex);
    
    // Verify file exists (basic check)
    FILE* f = fopen(file_path, "rb");
    if (!f) return OPENPANO_ERROR_FILE_NOT_FOUND;
    fclose(f);
    
    handle->image_paths.push_back(file_path);
    return OPENPANO_OK;
}

OPENPANO_API OpenpanoError openpano_add_image_data(
    OpenpanoStitcherHandle handle,
    const uint8_t* pixels,
    int32_t width,
    int32_t height,
    int32_t stride
) {
    if (!handle) return OPENPANO_ERROR_INVALID_HANDLE;
    if (!pixels || width <= 0 || height <= 0) return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    std::lock_guard<std::mutex> lock(handle->mutex);
    
    // Store image data - implementation depends on OpenPano internals
    // This is a placeholder showing the pattern
    try {
        // Create ImageRef from raw data...
        // handle->image_data.push_back(std::make_unique<pano::ImageRef>(...));
        return OPENPANO_OK;
    } catch (...) {
        return OPENPANO_ERROR_OUT_OF_MEMORY;
    }
}

OPENPANO_API OpenpanoError openpano_clear_images(OpenpanoStitcherHandle handle) {
    if (!handle) return OPENPANO_ERROR_INVALID_HANDLE;
    
    std::lock_guard<std::mutex> lock(handle->mutex);
    handle->image_paths.clear();
    handle->image_data.clear();
    return OPENPANO_OK;
}

OPENPANO_API int32_t openpano_image_count(OpenpanoStitcherHandle handle) {
    if (!handle) return -1;
    
    std::lock_guard<std::mutex> lock(handle->mutex);
    return static_cast<int32_t>(handle->image_paths.size() + handle->image_data.size());
}

/* ============================================================================
 * Stitching execution
 * ============================================================================ */

OPENPANO_API OpenpanoError openpano_stitch(
    OpenpanoStitcherHandle handle,
    OpenpanoImageHandle* result
) {
    return openpano_stitch_with_progress(handle, result, nullptr, nullptr);
}

OPENPANO_API OpenpanoError openpano_stitch_with_progress(
    OpenpanoStitcherHandle handle,
    OpenpanoImageHandle* result,
    OpenpanoProgressCallback callback,
    void* user_data
) {
    if (!handle) return OPENPANO_ERROR_INVALID_HANDLE;
    if (!result) return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    *result = nullptr;
    handle->cancelled = false;
    
    int image_count = openpano_image_count(handle);
    if (image_count < 2) return OPENPANO_ERROR_INSUFFICIENT_IMAGES;
    
    try {
        // Configure OpenPano based on settings
        // This is a placeholder - actual implementation depends on OpenPano API
        
        if (callback) callback(0.1f, "Detecting features", user_data);
        
        // Feature detection...
        if (handle->cancelled) return OPENPANO_ERROR_STITCHING_FAILED;
        
        if (callback) callback(0.3f, "Matching features", user_data);
        
        // Feature matching...
        if (handle->cancelled) return OPENPANO_ERROR_STITCHING_FAILED;
        
        if (callback) callback(0.5f, "Estimating transforms", user_data);
        
        // Transform estimation...
        if (handle->cancelled) return OPENPANO_ERROR_STITCHING_FAILED;
        
        if (callback) callback(0.7f, "Blending images", user_data);
        
        // Blending...
        if (handle->cancelled) return OPENPANO_ERROR_STITCHING_FAILED;
        
        if (callback) callback(0.9f, "Finalizing", user_data);
        
        // Create result
        auto* img = new OpenpanoImage();
        // Fill in result data...
        
        if (callback) callback(1.0f, "Complete", user_data);
        
        *result = img;
        return OPENPANO_OK;
        
    } catch (const std::bad_alloc&) {
        return OPENPANO_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return OPENPANO_ERROR_STITCHING_FAILED;
    }
}

OPENPANO_API OpenpanoError openpano_cancel(OpenpanoStitcherHandle handle) {
    if (!handle) return OPENPANO_ERROR_INVALID_HANDLE;
    handle->cancelled = true;
    return OPENPANO_OK;
}

/* ============================================================================
 * Result image handling
 * ============================================================================ */

OPENPANO_API OpenpanoError openpano_image_dimensions(
    OpenpanoImageHandle image,
    int32_t* width,
    int32_t* height
) {
    if (!image) return OPENPANO_ERROR_INVALID_HANDLE;
    if (!width || !height) return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    *width = image->width;
    *height = image->height;
    return OPENPANO_OK;
}

OPENPANO_API OpenpanoError openpano_image_data(
    OpenpanoImageHandle image,
    const uint8_t** pixels,
    size_t* size
) {
    if (!image) return OPENPANO_ERROR_INVALID_HANDLE;
    if (!pixels || !size) return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    *pixels = image->pixels.data();
    *size = image->pixels.size();
    return OPENPANO_OK;
}

OPENPANO_API OpenpanoError openpano_image_copy_data(
    OpenpanoImageHandle image,
    uint8_t* buffer,
    size_t buffer_size
) {
    if (!image) return OPENPANO_ERROR_INVALID_HANDLE;
    if (!buffer) return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    size_t copy_size = std::min(buffer_size, image->pixels.size());
    std::memcpy(buffer, image->pixels.data(), copy_size);
    return OPENPANO_OK;
}

OPENPANO_API OpenpanoError openpano_image_save(
    OpenpanoImageHandle image,
    const char* file_path,
    int quality
) {
    if (!image) return OPENPANO_ERROR_INVALID_HANDLE;
    if (!file_path) return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    // Implementation using stb_image_write or libjpeg/libpng
    // Placeholder...
    
    return OPENPANO_OK;
}

OPENPANO_API void openpano_image_destroy(OpenpanoImageHandle image) {
    delete image;
}

/* ============================================================================
 * Utility functions
 * ============================================================================ */

OPENPANO_API const char* openpano_version(void) {
    return "1.0.0";
}

OPENPANO_API int openpano_has_jpeg_support(void) {
#ifdef HAS_JPEG
    return 1;
#else
    return 0;
#endif
}

OPENPANO_API int openpano_has_png_support(void) {
#ifdef HAS_PNG
    return 1;
#else
    return 0;
#endif
}

// Global log state
static int g_log_level = 2;
static OpenpanoLogCallback g_log_callback = nullptr;
static void* g_log_user_data = nullptr;

OPENPANO_API void openpano_set_log_level(int level) {
    g_log_level = level;
}

OPENPANO_API void openpano_set_log_callback(
    OpenpanoLogCallback callback,
    void* user_data
) {
    g_log_callback = callback;
    g_log_user_data = user_data;
}
```

---

## Platform-Specific Integration

### Library Output by Platform

| Platform | Static Library | Shared Library | Notes |
|----------|---------------|----------------|-------|
| macOS | `libopenpano.a` | `libopenpano.dylib` | Universal binary recommended |
| Linux | `libopenpano.a` | `libopenpano.so` | |
| Windows | `openpano.lib` | `openpano.dll` | Need `.lib` import library |
| iOS | `libopenpano.a` | N/A | Static only, use XCFramework |
| Android | `libopenpano.a` | `libopenpano.so` | Per-ABI builds needed |

### macOS Universal Binary

```bash
# Build for both architectures
cmake -B build-x64 -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake -B build-arm64 -DCMAKE_OSX_ARCHITECTURES=arm64

cmake --build build-x64
cmake --build build-arm64

# Combine into universal binary
lipo -create \
    build-x64/libopenpano.a \
    build-arm64/libopenpano.a \
    -output libopenpano-universal.a
```

### iOS XCFramework

```bash
# Build for iOS device and simulator
cmake -B build-ios-device \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0

cmake -B build-ios-sim \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT=iphonesimulator \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"

cmake --build build-ios-device
cmake --build build-ios-sim

# Create XCFramework
xcodebuild -create-xcframework \
    -library build-ios-device/libopenpano.a \
    -headers include/ \
    -library build-ios-sim/libopenpano.a \
    -headers include/ \
    -output OpenPano.xcframework
```

---

## Rust FFI Integration

### Cargo.toml

```toml
[package]
name = "openpano-rs"
version = "0.1.0"
edition = "2021"

[dependencies]
thiserror = "1.0"

[build-dependencies]
bindgen = "0.69"
cc = "1.0"
```

### build.rs

```rust
use std::env;
use std::path::PathBuf;

fn main() {
    // Link to OpenPano library
    println!("cargo:rustc-link-search=native=./openpano/build");
    println!("cargo:rustc-link-lib=static=openpano");
    
    // Link system dependencies
    #[cfg(target_os = "macos")]
    {
        println!("cargo:rustc-link-lib=c++");
        println!("cargo:rustc-link-lib=framework=Accelerate");
    }
    
    #[cfg(target_os = "linux")]
    {
        println!("cargo:rustc-link-lib=stdc++");
    }
    
    // Generate bindings
    let bindings = bindgen::Builder::default()
        .header("openpano/include/openpano_ffi.h")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings");
    
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
```

### src/lib.rs - Safe Rust Wrapper

```rust
//! Safe Rust bindings for OpenPano panorama stitching library
//!
//! # Example
//! ```no_run
//! use openpano_rs::{Stitcher, StitchMode, Config};
//!
//! let config = Config::default()
//!     .mode(StitchMode::EstimateCamera)
//!     .crop(true);
//!
//! let mut stitcher = Stitcher::new(config)?;
//! stitcher.add_image("image1.jpg")?;
//! stitcher.add_image("image2.jpg")?;
//!
//! let result = stitcher.stitch()?;
//! result.save("panorama.jpg", 95)?;
//! ```

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use std::ffi::{CStr, CString};
use std::path::Path;
use std::ptr;
use thiserror::Error;

// Include generated bindings
include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

/// Errors that can occur during panorama stitching
#[derive(Error, Debug)]
pub enum OpenPanoError {
    #[error("Invalid handle")]
    InvalidHandle,
    
    #[error("Invalid argument: {0}")]
    InvalidArgument(String),
    
    #[error("File not found: {0}")]
    FileNotFound(String),
    
    #[error("Need at least 2 images for stitching")]
    InsufficientImages,
    
    #[error("Stitching failed")]
    StitchingFailed,
    
    #[error("Out of memory")]
    OutOfMemory,
    
    #[error("Feature detection failed")]
    FeatureDetectionFailed,
    
    #[error("Feature matching failed")]
    MatchingFailed,
    
    #[error("Unknown error: {0}")]
    Unknown(i32),
}

impl From<OpenpanoError> for OpenPanoError {
    fn from(err: OpenpanoError) -> Self {
        match err {
            OpenpanoError_OPENPANO_OK => panic!("OK is not an error"),
            OpenpanoError_OPENPANO_ERROR_INVALID_HANDLE => OpenPanoError::InvalidHandle,
            OpenpanoError_OPENPANO_ERROR_INVALID_ARGUMENT => {
                OpenPanoError::InvalidArgument("Unknown".into())
            }
            OpenpanoError_OPENPANO_ERROR_FILE_NOT_FOUND => {
                OpenPanoError::FileNotFound("Unknown".into())
            }
            OpenpanoError_OPENPANO_ERROR_INSUFFICIENT_IMAGES => OpenPanoError::InsufficientImages,
            OpenpanoError_OPENPANO_ERROR_STITCHING_FAILED => OpenPanoError::StitchingFailed,
            OpenpanoError_OPENPANO_ERROR_OUT_OF_MEMORY => OpenPanoError::OutOfMemory,
            OpenpanoError_OPENPANO_ERROR_FEATURE_DETECTION_FAILED => {
                OpenPanoError::FeatureDetectionFailed
            }
            OpenpanoError_OPENPANO_ERROR_MATCHING_FAILED => OpenPanoError::MatchingFailed,
            code => OpenPanoError::Unknown(code),
        }
    }
}

pub type Result<T> = std::result::Result<T, OpenPanoError>;

/// Stitching mode
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StitchMode {
    /// Cylinder projection - camera rotates around vertical axis
    Cylinder,
    /// Camera estimation with bundle adjustment
    EstimateCamera,
    /// Pure translation between images
    Translation,
}

impl From<StitchMode> for OpenpanoMode {
    fn from(mode: StitchMode) -> Self {
        match mode {
            StitchMode::Cylinder => OpenpanoMode_OPENPANO_MODE_CYLINDER,
            StitchMode::EstimateCamera => OpenpanoMode_OPENPANO_MODE_ESTIMATE_CAMERA,
            StitchMode::Translation => OpenpanoMode_OPENPANO_MODE_TRANSLATION,
        }
    }
}

/// Configuration for panorama stitching
#[derive(Debug, Clone)]
pub struct Config {
    inner: OpenpanoConfig,
}

impl Default for Config {
    fn default() -> Self {
        let mut inner = OpenpanoConfig {
            mode: OpenpanoMode_OPENPANO_MODE_ESTIMATE_CAMERA,
            focal_length: 0.0,
            ordered_input: 0,
            crop_result: 0,
            num_threads: 0,
            lazy_read: 0,
            ransac_confidence: 0.0,
            min_matches: 0,
        };
        unsafe {
            openpano_config_default(&mut inner);
        }
        Self { inner }
    }
}

impl Config {
    /// Set the stitching mode
    pub fn mode(mut self, mode: StitchMode) -> Self {
        self.inner.mode = mode.into();
        self
    }
    
    /// Set focal length (35mm equivalent, for cylinder mode)
    pub fn focal_length(mut self, focal: f32) -> Self {
        self.inner.focal_length = focal;
        self
    }
    
    /// Set whether input images are in order
    pub fn ordered(mut self, ordered: bool) -> Self {
        self.inner.ordered_input = ordered as i32;
        self
    }
    
    /// Set whether to crop result
    pub fn crop(mut self, crop: bool) -> Self {
        self.inner.crop_result = crop as i32;
        self
    }
    
    /// Set number of threads (0 = auto)
    pub fn threads(mut self, threads: u32) -> Self {
        self.inner.num_threads = threads as i32;
        self
    }
}

/// Progress callback for stitching operations
pub type ProgressCallback = Box<dyn Fn(f32, &str) + Send>;

/// Panorama stitcher
pub struct Stitcher {
    handle: OpenpanoStitcherHandle,
}

// Safety: Stitcher handle is thread-safe (protected by internal mutex)
unsafe impl Send for Stitcher {}
unsafe impl Sync for Stitcher {}

impl Stitcher {
    /// Create a new stitcher with the given configuration
    pub fn new(config: Config) -> Result<Self> {
        let handle = unsafe { openpano_stitcher_create(&config.inner) };
        if handle.is_null() {
            return Err(OpenPanoError::OutOfMemory);
        }
        Ok(Self { handle })
    }
    
    /// Create a new stitcher with default configuration
    pub fn with_defaults() -> Result<Self> {
        Self::new(Config::default())
    }
    
    /// Add an image file to the stitcher
    pub fn add_image<P: AsRef<Path>>(&mut self, path: P) -> Result<()> {
        let path_str = path.as_ref().to_string_lossy();
        let c_path = CString::new(path_str.as_ref())
            .map_err(|_| OpenPanoError::InvalidArgument("Invalid path".into()))?;
        
        let err = unsafe { openpano_add_image_file(self.handle, c_path.as_ptr()) };
        
        if err == OpenpanoError_OPENPANO_OK {
            Ok(())
        } else if err == OpenpanoError_OPENPANO_ERROR_FILE_NOT_FOUND {
            Err(OpenPanoError::FileNotFound(path_str.into_owned()))
        } else {
            Err(err.into())
        }
    }
    
    /// Add an image from raw RGB pixel data
    pub fn add_image_data(&mut self, pixels: &[u8], width: u32, height: u32) -> Result<()> {
        let err = unsafe {
            openpano_add_image_data(
                self.handle,
                pixels.as_ptr(),
                width as i32,
                height as i32,
                0, // auto stride
            )
        };
        
        if err == OpenpanoError_OPENPANO_OK {
            Ok(())
        } else {
            Err(err.into())
        }
    }
    
    /// Get the number of images added
    pub fn image_count(&self) -> usize {
        let count = unsafe { openpano_image_count(self.handle) };
        count.max(0) as usize
    }
    
    /// Clear all added images
    pub fn clear(&mut self) -> Result<()> {
        let err = unsafe { openpano_clear_images(self.handle) };
        if err == OpenpanoError_OPENPANO_OK {
            Ok(())
        } else {
            Err(err.into())
        }
    }
    
    /// Execute panorama stitching
    pub fn stitch(&mut self) -> Result<Image> {
        let mut result: OpenpanoImageHandle = ptr::null_mut();
        let err = unsafe { openpano_stitch(self.handle, &mut result) };
        
        if err == OpenpanoError_OPENPANO_OK && !result.is_null() {
            Ok(Image { handle: result })
        } else {
            Err(err.into())
        }
    }
    
    /// Execute panorama stitching with progress callback
    pub fn stitch_with_progress<F>(&mut self, callback: F) -> Result<Image>
    where
        F: Fn(f32, &str) + 'static,
    {
        extern "C" fn progress_trampoline(
            progress: f32,
            stage: *const std::os::raw::c_char,
            user_data: *mut std::os::raw::c_void,
        ) {
            unsafe {
                let callback = &*(user_data as *const Box<dyn Fn(f32, &str)>);
                let stage_str = if stage.is_null() {
                    ""
                } else {
                    CStr::from_ptr(stage).to_str().unwrap_or("")
                };
                callback(progress, stage_str);
            }
        }
        
        let callback_box: Box<Box<dyn Fn(f32, &str)>> = Box::new(Box::new(callback));
        let user_data = Box::into_raw(callback_box) as *mut std::os::raw::c_void;
        
        let mut result: OpenpanoImageHandle = ptr::null_mut();
        let err = unsafe {
            openpano_stitch_with_progress(
                self.handle,
                &mut result,
                Some(progress_trampoline),
                user_data,
            )
        };
        
        // Clean up callback
        unsafe {
            drop(Box::from_raw(user_data as *mut Box<dyn Fn(f32, &str)>));
        }
        
        if err == OpenpanoError_OPENPANO_OK && !result.is_null() {
            Ok(Image { handle: result })
        } else {
            Err(err.into())
        }
    }
    
    /// Cancel an ongoing stitching operation
    pub fn cancel(&self) -> Result<()> {
        let err = unsafe { openpano_cancel(self.handle) };
        if err == OpenpanoError_OPENPANO_OK {
            Ok(())
        } else {
            Err(err.into())
        }
    }
}

impl Drop for Stitcher {
    fn drop(&mut self) {
        unsafe {
            openpano_stitcher_destroy(self.handle);
        }
    }
}

/// Result image from stitching
pub struct Image {
    handle: OpenpanoImageHandle,
}

unsafe impl Send for Image {}
unsafe impl Sync for Image {}

impl Image {
    /// Get image dimensions (width, height)
    pub fn dimensions(&self) -> Result<(u32, u32)> {
        let mut width: i32 = 0;
        let mut height: i32 = 0;
        
        let err = unsafe { openpano_image_dimensions(self.handle, &mut width, &mut height) };
        
        if err == OpenpanoError_OPENPANO_OK {
            Ok((width as u32, height as u32))
        } else {
            Err(err.into())
        }
    }
    
    /// Get a reference to the pixel data (RGB, 8-bit per channel)
    pub fn pixels(&self) -> Result<&[u8]> {
        let mut pixels: *const u8 = ptr::null();
        let mut size: usize = 0;
        
        let err = unsafe { openpano_image_data(self.handle, &mut pixels, &mut size) };
        
        if err == OpenpanoError_OPENPANO_OK && !pixels.is_null() {
            Ok(unsafe { std::slice::from_raw_parts(pixels, size) })
        } else {
            Err(err.into())
        }
    }
    
    /// Copy pixel data to a Vec
    pub fn to_vec(&self) -> Result<Vec<u8>> {
        Ok(self.pixels()?.to_vec())
    }
    
    /// Save image to file
    pub fn save<P: AsRef<Path>>(&self, path: P, quality: u8) -> Result<()> {
        let path_str = path.as_ref().to_string_lossy();
        let c_path = CString::new(path_str.as_ref())
            .map_err(|_| OpenPanoError::InvalidArgument("Invalid path".into()))?;
        
        let err = unsafe { openpano_image_save(self.handle, c_path.as_ptr(), quality as i32) };
        
        if err == OpenpanoError_OPENPANO_OK {
            Ok(())
        } else {
            Err(err.into())
        }
    }
}

impl Drop for Image {
    fn drop(&mut self) {
        unsafe {
            openpano_image_destroy(self.handle);
        }
    }
}

/// Get OpenPano library version
pub fn version() -> &'static str {
    unsafe {
        let ptr = openpano_version();
        CStr::from_ptr(ptr).to_str().unwrap_or("unknown")
    }
}

/// Check if JPEG support is available
pub fn has_jpeg_support() -> bool {
    unsafe { openpano_has_jpeg_support() != 0 }
}

/// Check if PNG support is available
pub fn has_png_support() -> bool {
    unsafe { openpano_has_png_support() != 0 }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_config_builder() {
        let config = Config::default()
            .mode(StitchMode::Cylinder)
            .focal_length(35.0)
            .crop(true);
        
        assert_eq!(config.inner.mode, OpenpanoMode_OPENPANO_MODE_CYLINDER);
    }
    
    #[test]
    fn test_stitcher_creation() {
        let stitcher = Stitcher::with_defaults();
        assert!(stitcher.is_ok());
    }
}
```

---

## Go FFI Integration (cgo)

### openpano.go

```go
package openpano

/*
#cgo CFLAGS: -I${SRCDIR}/openpano/include
#cgo LDFLAGS: -L${SRCDIR}/openpano/build -lopenpano -lstdc++ -lm

#cgo darwin LDFLAGS: -framework Accelerate
#cgo linux LDFLAGS: -lpthread

#include "openpano_ffi.h"
#include <stdlib.h>
*/
import "C"

import (
	"errors"
	"runtime"
	"unsafe"
)

// StitchMode represents the stitching algorithm mode
type StitchMode int

const (
	// ModeCylinder - camera rotates around vertical axis
	ModeCylinder StitchMode = C.OPENPANO_MODE_CYLINDER
	// ModeEstimateCamera - bundle adjustment for arbitrary poses
	ModeEstimateCamera StitchMode = C.OPENPANO_MODE_ESTIMATE_CAMERA
	// ModeTranslation - pure translation between images
	ModeTranslation StitchMode = C.OPENPANO_MODE_TRANSLATION
)

// Config holds stitching configuration options
type Config struct {
	Mode            StitchMode
	FocalLength     float32
	OrderedInput    bool
	CropResult      bool
	NumThreads      int
	LazyRead        bool
	RansacConfidence float32
	MinMatches      int
}

// DefaultConfig returns sensible default configuration
func DefaultConfig() Config {
	var c C.OpenpanoConfig
	C.openpano_config_default(&c)
	
	return Config{
		Mode:            StitchMode(c.mode),
		FocalLength:     float32(c.focal_length),
		OrderedInput:    c.ordered_input != 0,
		CropResult:      c.crop_result != 0,
		NumThreads:      int(c.num_threads),
		LazyRead:        c.lazy_read != 0,
		RansacConfidence: float32(c.ransac_confidence),
		MinMatches:      int(c.min_matches),
	}
}

func (c Config) toC() C.OpenpanoConfig {
	ordered := C.int(0)
	if c.OrderedInput {
		ordered = 1
	}
	crop := C.int(0)
	if c.CropResult {
		crop = 1
	}
	lazy := C.int(0)
	if c.LazyRead {
		lazy = 1
	}
	
	return C.OpenpanoConfig{
		mode:             C.OpenpanoMode(c.Mode),
		focal_length:     C.float(c.FocalLength),
		ordered_input:    ordered,
		crop_result:      crop,
		num_threads:      C.int(c.NumThreads),
		lazy_read:        lazy,
		ransac_confidence: C.float(c.RansacConfidence),
		min_matches:      C.int(c.MinMatches),
	}
}

// Error codes
var (
	ErrInvalidHandle     = errors.New("invalid handle")
	ErrInvalidArgument   = errors.New("invalid argument")
	ErrFileNotFound      = errors.New("file not found")
	ErrInsufficientImages = errors.New("need at least 2 images")
	ErrStitchingFailed   = errors.New("stitching failed")
	ErrOutOfMemory       = errors.New("out of memory")
	ErrFeatureDetection  = errors.New("feature detection failed")
	ErrMatching          = errors.New("feature matching failed")
	ErrUnknown           = errors.New("unknown error")
)

func errorFromC(err C.OpenpanoError) error {
	switch err {
	case C.OPENPANO_OK:
		return nil
	case C.OPENPANO_ERROR_INVALID_HANDLE:
		return ErrInvalidHandle
	case C.OPENPANO_ERROR_INVALID_ARGUMENT:
		return ErrInvalidArgument
	case C.OPENPANO_ERROR_FILE_NOT_FOUND:
		return ErrFileNotFound
	case C.OPENPANO_ERROR_INSUFFICIENT_IMAGES:
		return ErrInsufficientImages
	case C.OPENPANO_ERROR_STITCHING_FAILED:
		return ErrStitchingFailed
	case C.OPENPANO_ERROR_OUT_OF_MEMORY:
		return ErrOutOfMemory
	case C.OPENPANO_ERROR_FEATURE_DETECTION_FAILED:
		return ErrFeatureDetection
	case C.OPENPANO_ERROR_MATCHING_FAILED:
		return ErrMatching
	default:
		return ErrUnknown
	}
}

// ProgressFunc is called during stitching to report progress
type ProgressFunc func(progress float32, stage string)

// Stitcher handles panorama stitching operations
type Stitcher struct {
	handle C.OpenpanoStitcherHandle
}

// NewStitcher creates a new stitcher with the given configuration
func NewStitcher(config Config) (*Stitcher, error) {
	cConfig := config.toC()
	handle := C.openpano_stitcher_create(&cConfig)
	if handle == nil {
		return nil, ErrOutOfMemory
	}
	
	s := &Stitcher{handle: handle}
	runtime.SetFinalizer(s, (*Stitcher).Close)
	return s, nil
}

// NewDefaultStitcher creates a stitcher with default configuration
func NewDefaultStitcher() (*Stitcher, error) {
	return NewStitcher(DefaultConfig())
}

// Close releases resources associated with the stitcher
func (s *Stitcher) Close() {
	if s.handle != nil {
		C.openpano_stitcher_destroy(s.handle)
		s.handle = nil
	}
}

// AddImage adds an image file to the stitcher
func (s *Stitcher) AddImage(path string) error {
	cPath := C.CString(path)
	defer C.free(unsafe.Pointer(cPath))
	
	err := C.openpano_add_image_file(s.handle, cPath)
	return errorFromC(err)
}

// AddImageData adds an image from raw RGB pixel data
func (s *Stitcher) AddImageData(pixels []byte, width, height int) error {
	if len(pixels) == 0 {
		return ErrInvalidArgument
	}
	
	err := C.openpano_add_image_data(
		s.handle,
		(*C.uint8_t)(unsafe.Pointer(&pixels[0])),
		C.int32_t(width),
		C.int32_t(height),
		0,
	)
	return errorFromC(err)
}

// ImageCount returns the number of images added
func (s *Stitcher) ImageCount() int {
	count := C.openpano_image_count(s.handle)
	if count < 0 {
		return 0
	}
	return int(count)
}

// Clear removes all added images
func (s *Stitcher) Clear() error {
	err := C.openpano_clear_images(s.handle)
	return errorFromC(err)
}

// Stitch executes panorama stitching and returns the result
func (s *Stitcher) Stitch() (*Image, error) {
	var result C.OpenpanoImageHandle
	err := C.openpano_stitch(s.handle, &result)
	if err != C.OPENPANO_OK {
		return nil, errorFromC(err)
	}
	
	img := &Image{handle: result}
	runtime.SetFinalizer(img, (*Image).Close)
	return img, nil
}

// Cancel aborts an ongoing stitching operation
func (s *Stitcher) Cancel() error {
	err := C.openpano_cancel(s.handle)
	return errorFromC(err)
}

// Image represents a result panorama image
type Image struct {
	handle C.OpenpanoImageHandle
}

// Close releases resources associated with the image
func (img *Image) Close() {
	if img.handle != nil {
		C.openpano_image_destroy(img.handle)
		img.handle = nil
	}
}

// Dimensions returns the image width and height
func (img *Image) Dimensions() (width, height int, err error) {
	var w, h C.int32_t
	cerr := C.openpano_image_dimensions(img.handle, &w, &h)
	if cerr != C.OPENPANO_OK {
		return 0, 0, errorFromC(cerr)
	}
	return int(w), int(h), nil
}

// Pixels returns a copy of the pixel data (RGB, 8-bit per channel)
func (img *Image) Pixels() ([]byte, error) {
	var pixels *C.uint8_t
	var size C.size_t
	
	err := C.openpano_image_data(img.handle, &pixels, &size)
	if err != C.OPENPANO_OK {
		return nil, errorFromC(err)
	}
	
	// Copy to Go slice
	result := make([]byte, size)
	copy(result, (*[1 << 30]byte)(unsafe.Pointer(pixels))[:size:size])
	return result, nil
}

// Save writes the image to a file
func (img *Image) Save(path string, quality int) error {
	cPath := C.CString(path)
	defer C.free(unsafe.Pointer(cPath))
	
	err := C.openpano_image_save(img.handle, cPath, C.int(quality))
	return errorFromC(err)
}

// Version returns the OpenPano library version string
func Version() string {
	return C.GoString(C.openpano_version())
}

// HasJPEGSupport returns true if JPEG format is supported
func HasJPEGSupport() bool {
	return C.openpano_has_jpeg_support() != 0
}

// HasPNGSupport returns true if PNG format is supported
func HasPNGSupport() bool {
	return C.openpano_has_png_support() != 0
}
```

---

## Mobile Integration

### iOS (Swift)

Create a bridging header and Swift wrapper:

**OpenPano-Bridging-Header.h:**
```c
#import "openpano_ffi.h"
```

**OpenPano.swift:**
```swift
import Foundation

/// Stitching mode options
public enum StitchMode {
    case cylinder
    case estimateCamera
    case translation
    
    var cValue: OpenpanoMode {
        switch self {
        case .cylinder: return OPENPANO_MODE_CYLINDER
        case .estimateCamera: return OPENPANO_MODE_ESTIMATE_CAMERA
        case .translation: return OPENPANO_MODE_TRANSLATION
        }
    }
}

/// Configuration for panorama stitching
public struct StitcherConfig {
    public var mode: StitchMode = .estimateCamera
    public var focalLength: Float = 36.0
    public var orderedInput: Bool = false
    public var cropResult: Bool = true
    public var numThreads: Int = 0
    
    public init() {}
    
    func toCConfig() -> OpenpanoConfig {
        var config = OpenpanoConfig()
        openpano_config_default(&config)
        config.mode = mode.cValue
        config.focal_length = focalLength
        config.ordered_input = orderedInput ? 1 : 0
        config.crop_result = cropResult ? 1 : 0
        config.num_threads = Int32(numThreads)
        return config
    }
}

/// Error types for OpenPano operations
public enum OpenPanoError: Error {
    case invalidHandle
    case invalidArgument
    case fileNotFound(String)
    case insufficientImages
    case stitchingFailed
    case outOfMemory
    case featureDetectionFailed
    case matchingFailed
    case unknown(Int32)
    
    init(from error: OpenpanoError) {
        switch error {
        case OPENPANO_ERROR_INVALID_HANDLE: self = .invalidHandle
        case OPENPANO_ERROR_INVALID_ARGUMENT: self = .invalidArgument
        case OPENPANO_ERROR_FILE_NOT_FOUND: self = .fileNotFound("")
        case OPENPANO_ERROR_INSUFFICIENT_IMAGES: self = .insufficientImages
        case OPENPANO_ERROR_STITCHING_FAILED: self = .stitchingFailed
        case OPENPANO_ERROR_OUT_OF_MEMORY: self = .outOfMemory
        case OPENPANO_ERROR_FEATURE_DETECTION_FAILED: self = .featureDetectionFailed
        case OPENPANO_ERROR_MATCHING_FAILED: self = .matchingFailed
        default: self = .unknown(error.rawValue)
        }
    }
}

/// Result image from stitching operation
public class PanoramaImage {
    private var handle: OpenpanoImageHandle?
    
    init(handle: OpenpanoImageHandle) {
        self.handle = handle
    }
    
    deinit {
        if let handle = handle {
            openpano_image_destroy(handle)
        }
    }
    
    /// Get image dimensions
    public var dimensions: (width: Int, height: Int)? {
        guard let handle = handle else { return nil }
        var width: Int32 = 0
        var height: Int32 = 0
        let err = openpano_image_dimensions(handle, &width, &height)
        guard err == OPENPANO_OK else { return nil }
        return (Int(width), Int(height))
    }
    
    /// Convert to UIImage
    public func toUIImage() -> UIImage? {
        guard let handle = handle,
              let dims = dimensions else { return nil }
        
        var pixels: UnsafePointer<UInt8>?
        var size: Int = 0
        
        let err = openpano_image_data(handle, &pixels, &size)
        guard err == OPENPANO_OK, let pixelData = pixels else { return nil }
        
        let data = Data(bytes: pixelData, count: size)
        let colorSpace = CGColorSpaceCreateDeviceRGB()
        
        guard let provider = CGDataProvider(data: data as CFData),
              let cgImage = CGImage(
                width: dims.width,
                height: dims.height,
                bitsPerComponent: 8,
                bitsPerPixel: 24,
                bytesPerRow: dims.width * 3,
                space: colorSpace,
                bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.none.rawValue),
                provider: provider,
                decode: nil,
                shouldInterpolate: true,
                intent: .defaultIntent
              ) else { return nil }
        
        return UIImage(cgImage: cgImage)
    }
    
    /// Save to file
    public func save(to path: String, quality: Int = 90) throws {
        guard let handle = handle else { throw OpenPanoError.invalidHandle }
        let err = openpano_image_save(handle, path, Int32(quality))
        guard err == OPENPANO_OK else { throw OpenPanoError(from: err) }
    }
}

/// Panorama stitcher
public class Stitcher {
    private var handle: OpenpanoStitcherHandle?
    
    /// Create a new stitcher with configuration
    public init(config: StitcherConfig = StitcherConfig()) throws {
        var cConfig = config.toCConfig()
        handle = openpano_stitcher_create(&cConfig)
        guard handle != nil else { throw OpenPanoError.outOfMemory }
    }
    
    deinit {
        if let handle = handle {
            openpano_stitcher_destroy(handle)
        }
    }
    
    /// Add an image file
    public func addImage(path: String) throws {
        guard let handle = handle else { throw OpenPanoError.invalidHandle }
        let err = openpano_add_image_file(handle, path)
        guard err == OPENPANO_OK else { throw OpenPanoError(from: err) }
    }
    
    /// Add image from UIImage
    public func addImage(_ image: UIImage) throws {
        guard let handle = handle,
              let cgImage = image.cgImage else {
            throw OpenPanoError.invalidArgument
        }
        
        let width = cgImage.width
        let height = cgImage.height
        let bytesPerRow = width * 3
        
        var pixelData = [UInt8](repeating: 0, count: height * bytesPerRow)
        let colorSpace = CGColorSpaceCreateDeviceRGB()
        
        guard let context = CGContext(
            data: &pixelData,
            width: width,
            height: height,
            bitsPerComponent: 8,
            bytesPerRow: bytesPerRow,
            space: colorSpace,
            bitmapInfo: CGImageAlphaInfo.none.rawValue
        ) else {
            throw OpenPanoError.invalidArgument
        }
        
        context.draw(cgImage, in: CGRect(x: 0, y: 0, width: width, height: height))
        
        let err = openpano_add_image_data(
            handle,
            &pixelData,
            Int32(width),
            Int32(height),
            0
        )
        guard err == OPENPANO_OK else { throw OpenPanoError(from: err) }
    }
    
    /// Number of images added
    public var imageCount: Int {
        guard let handle = handle else { return 0 }
        return Int(openpano_image_count(handle))
    }
    
    /// Execute stitching
    public func stitch() throws -> PanoramaImage {
        guard let handle = handle else { throw OpenPanoError.invalidHandle }
        
        var result: OpenpanoImageHandle?
        let err = openpano_stitch(handle, &result)
        
        guard err == OPENPANO_OK, let imageHandle = result else {
            throw OpenPanoError(from: err)
        }
        
        return PanoramaImage(handle: imageHandle)
    }
    
    /// Cancel ongoing operation
    public func cancel() {
        guard let handle = handle else { return }
        openpano_cancel(handle)
    }
}

/// Get library version
public func openPanoVersion() -> String {
    return String(cString: openpano_version())
}
```

### Android (Kotlin)

**OpenPano.kt:**
```kotlin
package com.example.openpano

import android.graphics.Bitmap
import java.nio.ByteBuffer

/**
 * JNI bridge to OpenPano native library
 */
object OpenPanoJNI {
    init {
        System.loadLibrary("openpano_jni")
    }
    
    external fun nativeCreateStitcher(config: LongArray): Long
    external fun nativeDestroyStitcher(handle: Long)
    external fun nativeAddImageFile(handle: Long, path: String): Int
    external fun nativeAddImageData(handle: Long, pixels: ByteArray, width: Int, height: Int): Int
    external fun nativeImageCount(handle: Long): Int
    external fun nativeClearImages(handle: Long): Int
    external fun nativeStitch(handle: Long): Long
    external fun nativeCancel(handle: Long): Int
    external fun nativeDestroyImage(handle: Long)
    external fun nativeImageDimensions(handle: Long): IntArray
    external fun nativeImageData(handle: Long): ByteArray?
    external fun nativeImageSave(handle: Long, path: String, quality: Int): Int
    external fun nativeVersion(): String
}

enum class StitchMode(val value: Int) {
    CYLINDER(0),
    ESTIMATE_CAMERA(1),
    TRANSLATION(2)
}

data class StitcherConfig(
    val mode: StitchMode = StitchMode.ESTIMATE_CAMERA,
    val focalLength: Float = 36f,
    val orderedInput: Boolean = false,
    val cropResult: Boolean = true,
    val numThreads: Int = 0
)

sealed class OpenPanoException(message: String) : Exception(message) {
    object InvalidHandle : OpenPanoException("Invalid handle")
    object InvalidArgument : OpenPanoException("Invalid argument")
    class FileNotFound(path: String) : OpenPanoException("File not found: $path")
    object InsufficientImages : OpenPanoException("Need at least 2 images")
    object StitchingFailed : OpenPanoException("Stitching failed")
    object OutOfMemory : OpenPanoException("Out of memory")
    object FeatureDetectionFailed : OpenPanoException("Feature detection failed")
    object MatchingFailed : OpenPanoException("Feature matching failed")
    class Unknown(code: Int) : OpenPanoException("Unknown error: $code")
}

/**
 * Result panorama image
 */
class PanoramaImage internal constructor(private var handle: Long) : AutoCloseable {
    
    val dimensions: Pair<Int, Int>
        get() {
            val dims = OpenPanoJNI.nativeImageDimensions(handle)
            return Pair(dims[0], dims[1])
        }
    
    fun toBitmap(): Bitmap? {
        val data = OpenPanoJNI.nativeImageData(handle) ?: return null
        val (width, height) = dimensions
        
        // Convert RGB to ARGB for Bitmap
        val argbData = IntArray(width * height)
        for (i in argbData.indices) {
            val r = data[i * 3].toInt() and 0xFF
            val g = data[i * 3 + 1].toInt() and 0xFF
            val b = data[i * 3 + 2].toInt() and 0xFF
            argbData[i] = (0xFF shl 24) or (r shl 16) or (g shl 8) or b
        }
        
        return Bitmap.createBitmap(argbData, width, height, Bitmap.Config.ARGB_8888)
    }
    
    fun save(path: String, quality: Int = 90) {
        val err = OpenPanoJNI.nativeImageSave(handle, path, quality)
        if (err != 0) throw errorFromCode(err)
    }
    
    override fun close() {
        if (handle != 0L) {
            OpenPanoJNI.nativeDestroyImage(handle)
            handle = 0L
        }
    }
}

/**
 * Panorama stitcher
 */
class Stitcher(config: StitcherConfig = StitcherConfig()) : AutoCloseable {
    
    private var handle: Long
    
    init {
        val configArray = longArrayOf(
            config.mode.value.toLong(),
            config.focalLength.toBits().toLong(),
            if (config.orderedInput) 1L else 0L,
            if (config.cropResult) 1L else 0L,
            config.numThreads.toLong()
        )
        handle = OpenPanoJNI.nativeCreateStitcher(configArray)
        if (handle == 0L) throw OpenPanoException.OutOfMemory
    }
    
    fun addImage(path: String) {
        val err = OpenPanoJNI.nativeAddImageFile(handle, path)
        if (err != 0) throw errorFromCode(err)
    }
    
    fun addImage(bitmap: Bitmap) {
        val width = bitmap.width
        val height = bitmap.height
        val pixels = IntArray(width * height)
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height)
        
        // Convert ARGB to RGB
        val rgbData = ByteArray(width * height * 3)
        for (i in pixels.indices) {
            rgbData[i * 3] = ((pixels[i] shr 16) and 0xFF).toByte()
            rgbData[i * 3 + 1] = ((pixels[i] shr 8) and 0xFF).toByte()
            rgbData[i * 3 + 2] = (pixels[i] and 0xFF).toByte()
        }
        
        val err = OpenPanoJNI.nativeAddImageData(handle, rgbData, width, height)
        if (err != 0) throw errorFromCode(err)
    }
    
    val imageCount: Int
        get() = OpenPanoJNI.nativeImageCount(handle)
    
    fun clear() {
        val err = OpenPanoJNI.nativeClearImages(handle)
        if (err != 0) throw errorFromCode(err)
    }
    
    fun stitch(): PanoramaImage {
        val result = OpenPanoJNI.nativeStitch(handle)
        if (result == 0L) throw OpenPanoException.StitchingFailed
        return PanoramaImage(result)
    }
    
    fun cancel() {
        OpenPanoJNI.nativeCancel(handle)
    }
    
    override fun close() {
        if (handle != 0L) {
            OpenPanoJNI.nativeDestroyStitcher(handle)
            handle = 0L
        }
    }
}

fun openPanoVersion(): String = OpenPanoJNI.nativeVersion()

private fun errorFromCode(code: Int): OpenPanoException = when (code) {
    -1 -> OpenPanoException.InvalidHandle
    -2 -> OpenPanoException.InvalidArgument
    -3 -> OpenPanoException.FileNotFound("")
    -4 -> OpenPanoException.InsufficientImages
    -5 -> OpenPanoException.StitchingFailed
    -6 -> OpenPanoException.OutOfMemory
    -7 -> OpenPanoException.FeatureDetectionFailed
    -8 -> OpenPanoException.MatchingFailed
    else -> OpenPanoException.Unknown(code)
}
```

---

## Cross-Platform Build System

### Directory Structure

```
openpano-portable/
├── CMakeLists.txt              # Root CMake config
├── cmake/
│   ├── iOS.cmake               # iOS toolchain
│   └── Android.cmake           # Android config
├── openpano/                   # OpenPano source (git submodule)
├── ffi/
│   ├── include/
│   │   └── openpano_ffi.h
│   └── src/
│       └── openpano_ffi.cpp
├── bindings/
│   ├── rust/
│   │   ├── Cargo.toml
│   │   ├── build.rs
│   │   └── src/lib.rs
│   ├── go/
│   │   └── openpano.go
│   ├── swift/
│   │   └── OpenPano.swift
│   └── kotlin/
│       └── OpenPano.kt
├── scripts/
│   ├── build-all.sh
│   ├── build-ios.sh
│   ├── build-android.sh
│   └── build-desktop.sh
└── README.md
```

### Build Script: `scripts/build-all.sh`

```bash
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
OUTPUT_DIR="$PROJECT_ROOT/dist"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Build for macOS (Universal)
build_macos() {
    log_info "Building for macOS (Universal)..."
    
    cmake -B "$BUILD_DIR/macos-x64" \
        -DCMAKE_OSX_ARCHITECTURES=x86_64 \
        -DCMAKE_BUILD_TYPE=Release \
        "$PROJECT_ROOT"
    cmake --build "$BUILD_DIR/macos-x64" --parallel
    
    cmake -B "$BUILD_DIR/macos-arm64" \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_BUILD_TYPE=Release \
        "$PROJECT_ROOT"
    cmake --build "$BUILD_DIR/macos-arm64" --parallel
    
    # Create universal binary
    mkdir -p "$OUTPUT_DIR/macos"
    lipo -create \
        "$BUILD_DIR/macos-x64/libopenpano.a" \
        "$BUILD_DIR/macos-arm64/libopenpano.a" \
        -output "$OUTPUT_DIR/macos/libopenpano.a"
    
    log_info "macOS build complete: $OUTPUT_DIR/macos/libopenpano.a"
}

# Build for Linux
build_linux() {
    log_info "Building for Linux..."
    
    cmake -B "$BUILD_DIR/linux" \
        -DCMAKE_BUILD_TYPE=Release \
        "$PROJECT_ROOT"
    cmake --build "$BUILD_DIR/linux" --parallel
    
    mkdir -p "$OUTPUT_DIR/linux"
    cp "$BUILD_DIR/linux/libopenpano.a" "$OUTPUT_DIR/linux/"
    
    log_info "Linux build complete: $OUTPUT_DIR/linux/libopenpano.a"
}

# Build for iOS
build_ios() {
    log_info "Building for iOS..."
    
    # Device (arm64)
    cmake -B "$BUILD_DIR/ios-device" \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
        -DCMAKE_BUILD_TYPE=Release \
        "$PROJECT_ROOT"
    cmake --build "$BUILD_DIR/ios-device" --parallel
    
    # Simulator (x64 + arm64)
    cmake -B "$BUILD_DIR/ios-sim" \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_SYSROOT=iphonesimulator \
        -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
        -DCMAKE_BUILD_TYPE=Release \
        "$PROJECT_ROOT"
    cmake --build "$BUILD_DIR/ios-sim" --parallel
    
    # Create XCFramework
    mkdir -p "$OUTPUT_DIR/ios"
    xcodebuild -create-xcframework \
        -library "$BUILD_DIR/ios-device/libopenpano.a" \
        -headers "$PROJECT_ROOT/ffi/include" \
        -library "$BUILD_DIR/ios-sim/libopenpano.a" \
        -headers "$PROJECT_ROOT/ffi/include" \
        -output "$OUTPUT_DIR/ios/OpenPano.xcframework"
    
    log_info "iOS build complete: $OUTPUT_DIR/ios/OpenPano.xcframework"
}

# Build for Android
build_android() {
    log_info "Building for Android..."
    
    if [ -z "$ANDROID_NDK" ]; then
        log_error "ANDROID_NDK environment variable not set"
        exit 1
    fi
    
    ABIS=("armeabi-v7a" "arm64-v8a" "x86" "x86_64")
    
    for ABI in "${ABIS[@]}"; do
        log_info "Building for Android ABI: $ABI"
        
        cmake -B "$BUILD_DIR/android-$ABI" \
            -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
            -DANDROID_ABI=$ABI \
            -DANDROID_PLATFORM=android-24 \
            -DCMAKE_BUILD_TYPE=Release \
            "$PROJECT_ROOT"
        cmake --build "$BUILD_DIR/android-$ABI" --parallel
        
        mkdir -p "$OUTPUT_DIR/android/$ABI"
        cp "$BUILD_DIR/android-$ABI/libopenpano.a" "$OUTPUT_DIR/android/$ABI/"
    done
    
    log_info "Android build complete: $OUTPUT_DIR/android/"
}

# Build for Windows (cross-compile with MinGW)
build_windows() {
    log_info "Building for Windows (MinGW)..."
    
    cmake -B "$BUILD_DIR/windows" \
        -DCMAKE_TOOLCHAIN_FILE="$PROJECT_ROOT/cmake/mingw-w64.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        "$PROJECT_ROOT"
    cmake --build "$BUILD_DIR/windows" --parallel
    
    mkdir -p "$OUTPUT_DIR/windows"
    cp "$BUILD_DIR/windows/libopenpano.a" "$OUTPUT_DIR/windows/"
    
    log_info "Windows build complete: $OUTPUT_DIR/windows/"
}

# Main
case "${1:-all}" in
    macos)  build_macos ;;
    linux)  build_linux ;;
    ios)    build_ios ;;
    android) build_android ;;
    windows) build_windows ;;
    all)
        build_macos
        build_ios
        build_android
        ;;
    *)
        echo "Usage: $0 [macos|linux|ios|android|windows|all]"
        exit 1
        ;;
esac

log_info "Build complete! Output in: $OUTPUT_DIR"
```

---

## Recommended Tech Stacks

### Option 1: Rust + Tauri 2.0 (Recommended)

**Pros:**
- Single codebase for desktop + mobile
- Web-based UI (HTML/CSS/JS or frameworks like React/Vue)
- Excellent Rust integration
- Small binary size

**Architecture:**
```
┌─────────────────────────────────────┐
│     Tauri Frontend (Web View)       │
│        React/Vue/Svelte             │
├─────────────────────────────────────┤
│     Tauri Backend (Rust)            │
│     ├── Commands/Events             │
│     └── openpano-rs bindings        │
├─────────────────────────────────────┤
│     OpenPano FFI (C interface)      │
├─────────────────────────────────────┤
│     OpenPano Core (C++)             │
└─────────────────────────────────────┘
```

### Option 2: Flutter + Rust (flutter_rust_bridge)

**Pros:**
- Excellent mobile UI framework
- Hot reload for rapid development
- Strong community

**Architecture:**
```
┌─────────────────────────────────────┐
│     Flutter UI (Dart)               │
├─────────────────────────────────────┤
│     flutter_rust_bridge             │
├─────────────────────────────────────┤
│     Rust Layer                      │
│     └── openpano-rs bindings        │
├─────────────────────────────────────┤
│     OpenPano FFI (C interface)      │
├─────────────────────────────────────┤
│     OpenPano Core (C++)             │
└─────────────────────────────────────┘
```

### Option 3: Native per Platform

**Pros:**
- Best native experience
- Full platform API access

**Architecture:**
```
Desktop:
  macOS: SwiftUI + Swift bindings
  Windows: WinUI 3 + C++/WinRT
  Linux: GTK4 + Rust bindings

Mobile:
  iOS: SwiftUI + Swift bindings
  Android: Jetpack Compose + Kotlin/JNI bindings
```

---

## Quick Start

1. **Clone repositories:**
```bash
git clone https://github.com/ppwwyyxx/OpenPano.git
```

2. **Add FFI layer:**
   - Copy `openpano_ffi.h` and `openpano_ffi.cpp` to project

3. **Build library:**
```bash
./scripts/build-all.sh macos  # or ios, android
```

4. **Integrate with your app:**
   - Use Rust, Go, Swift, or Kotlin bindings provided above

---

## References

- [OpenPano GitHub](https://github.com/ppwwyyxx/OpenPano)
- [Tauri Documentation](https://tauri.app/)
- [flutter_rust_bridge](https://github.com/aspect-build/aspect-cli)
- [Rust FFI Guide](https://doc.rust-lang.org/nomicon/ffi.html)
- [Android NDK Guide](https://developer.android.com/ndk/guides)
- [iOS Framework Guide](https://developer.apple.com/documentation/xcode/creating-a-multi-platform-binary-framework-bundle)

---

*Document Version: 1.0*  
*Last Updated: December 2024*

