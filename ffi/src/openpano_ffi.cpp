/**
 * @file openpano_ffi.cpp
 * @brief Implementation of C-compatible FFI bindings for OpenPano
 * 
 * This file provides the bridge between the C FFI interface and the
 * underlying C++ OpenPano library.
 */

#include "openpano_ffi.h"

// OpenPano headers
#include "stitch/stitcher.hh"
#include "stitch/cylstitcher.hh"
#include "lib/config.hh"
#include "lib/imgproc.hh"
#include "lib/mat.h"
#include "lib/timer.hh"
#include "stitch/imageref.hh"
#include "common/common.hh"

#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <cstring>
#include <cstdio>
#include <thread>
#include <algorithm>

/* ============================================================================
 * Version info
 * ============================================================================ */

#define OPENPANO_FFI_VERSION "1.0.0"

/* ============================================================================
 * Internal structures
 * ============================================================================ */

/**
 * @brief Internal stitcher state
 * 
 * Manages image collection and configuration for a stitching session.
 */
struct OpenpanoStitcher {
    std::vector<std::string> image_paths;
    std::vector<std::unique_ptr<Mat32f>> image_data;
    OpenpanoConfig config;
    std::atomic<bool> cancelled{false};
    std::mutex mutex;
    
    /** Default constructor initializes with default config */
    OpenpanoStitcher() {
        openpano_config_default(&config);
    }
};

/**
 * @brief Internal image result wrapper
 * 
 * Holds the stitched panorama result with both float and byte formats.
 */
struct OpenpanoImage {
    Mat32f mat_float;              /**< Original float format from OpenPano */
    std::vector<uint8_t> pixels_byte;    /**< Converted 8-bit RGB cache */
    bool byte_cache_valid{false};        /**< Whether byte cache is up-to-date */
    
    /**
     * @brief Constructor from OpenPano Mat32f
     * @param m Source matrix (moved into this structure)
     */
    explicit OpenpanoImage(Mat32f&& m) : mat_float(std::move(m)) {}
    
    /**
     * @brief Ensure 8-bit pixel cache is valid
     * 
     * Converts float [0,1] to uint8 [0,255] on demand.
     */
    void ensure_byte_cache() {
        if (byte_cache_valid) return;
        
        int w = mat_float.width();
        int h = mat_float.height();
        int c = mat_float.channels();
        
        pixels_byte.resize(w * h * c);
        
        const float* src = mat_float.ptr(0);
        for (size_t i = 0; i < pixels_byte.size(); ++i) {
            float val = src[i];
            // Clamp to [0,1] and convert to [0,255]
            if (val < 0.0f) val = 0.0f;
            if (val > 1.0f) val = 1.0f;
            pixels_byte[i] = static_cast<uint8_t>(val * 255.0f + 0.5f);
        }
        
        byte_cache_valid = true;
    }
};

/* ============================================================================
 * Global state
 * ============================================================================ */

static OpenpanoLogLevel g_log_level = OPENPANO_LOG_WARN;
static OpenpanoLogCallback g_log_callback = nullptr;
static void* g_log_user_data = nullptr;
static std::mutex g_log_mutex;

/**
 * @brief Internal logging function
 * 
 * @param level Log level
 * @param fmt Printf-style format string
 */
static void openpano_log(OpenpanoLogLevel level, const char* fmt, ...) {
    if (level > g_log_level) return;
    
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_log_callback) {
        g_log_callback(level, buffer, g_log_user_data);
    } else {
        fprintf(stderr, "[OpenPano] %s\n", buffer);
    }
}

/* ============================================================================
 * Error handling
 * ============================================================================ */

OPENPANO_API const char* openpano_error_string(OpenpanoError error) {
    switch (error) {
        case OPENPANO_OK: 
            return "Success";
        case OPENPANO_ERROR_INVALID_HANDLE: 
            return "Invalid handle";
        case OPENPANO_ERROR_INVALID_ARGUMENT: 
            return "Invalid argument";
        case OPENPANO_ERROR_FILE_NOT_FOUND: 
            return "File not found";
        case OPENPANO_ERROR_INSUFFICIENT_IMAGES: 
            return "Insufficient images (need at least 2)";
        case OPENPANO_ERROR_STITCHING_FAILED: 
            return "Stitching failed";
        case OPENPANO_ERROR_OUT_OF_MEMORY: 
            return "Out of memory";
        case OPENPANO_ERROR_FEATURE_DETECTION_FAILED: 
            return "Feature detection failed";
        case OPENPANO_ERROR_MATCHING_FAILED: 
            return "Feature matching failed";
        case OPENPANO_ERROR_UNSUPPORTED_FORMAT:
            return "Unsupported image format";
        case OPENPANO_ERROR_CANCELLED:
            return "Operation cancelled";
        default: 
            return "Unknown error";
    }
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

OPENPANO_API void openpano_config_default(OpenpanoConfig* config) {
    if (!config) return;
    
    std::memset(config, 0, sizeof(OpenpanoConfig));
    
    config->mode = OPENPANO_MODE_ESTIMATE_CAMERA;  // = 1 (not 0!)
    config->focal_length = 36.0f;  // 35mm equivalent
    config->ordered_input = 0;     // auto-detect
    config->crop_result = 1;       // enabled
    config->num_threads = 0;       // auto
    config->lazy_read = 0;         // disabled for safety
    config->straighten = 0;        // disabled
    config->max_output_size = 8000; // maximum dimension (0 causes division by zero!)
    config->multiband = 1;         // enabled
}

/**
 * @brief Apply FFI config to OpenPano global config
 * 
 * This sets the global config variables used by OpenPano internals.
 * 
 * @param cfg FFI configuration to apply
 */
static void apply_config_to_openpano(const OpenpanoConfig* cfg) {
    if (!cfg) return;
    
    // Set mode flags
    config::CYLINDER = (cfg->mode == OPENPANO_MODE_CYLINDER);
    config::TRANS = (cfg->mode == OPENPANO_MODE_TRANSLATION);
    config::ESTIMATE_CAMERA = (cfg->mode == OPENPANO_MODE_ESTIMATE_CAMERA);
    
    // Set other options
    config::FOCAL_LENGTH = cfg->focal_length;
    config::ORDERED_INPUT = (cfg->ordered_input != 0);
    config::CROP = (cfg->crop_result != 0);
    config::LAZY_READ = (cfg->lazy_read != 0);
    config::STRAIGHTEN = (cfg->straighten != 0);
    config::MAX_OUTPUT_SIZE = cfg->max_output_size;
    config::MULTIBAND = cfg->multiband;
    
    // Initialize SIFT and feature detection parameters (from config.cfg defaults)
    config::SIFT_WORKING_SIZE = 800;
    config::NUM_OCTAVE = 4;
    config::NUM_SCALE = 7;
    config::SCALE_FACTOR = 1.4142135623f;
    config::GAUSS_SIGMA = 1.4142135623f;
    config::GAUSS_WINDOW_FACTOR = 6;
    config::CONTRAST_THRES = 4e-2f;
    config::JUDGE_EXTREMA_DIFF_THRES = 2e-3f;
    config::EDGE_RATIO = 6.0f;
    config::PRE_COLOR_THRES = 5e-2f;
    config::CALC_OFFSET_DEPTH = 4;
    config::OFFSET_THRES = 0.5f;
    
    // Descriptor and matching parameters
    config::ORI_RADIUS = 4.5f;
    config::ORI_HIST_SMOOTH_COUNT = 2;
    config::DESC_HIST_SCALE_FACTOR = 3;
    config::DESC_INT_FACTOR = 512;
    config::MATCH_REJECT_NEXT_RATIO = 0.8f;
    
    // RANSAC parameters
    config::RANSAC_ITERATIONS = 1500;
    config::RANSAC_INLIER_THRES = 3.5;
    config::INLIER_IN_MATCH_RATIO = 0.1f;
    config::INLIER_IN_POINTS_RATIO = 0.04f;
    
    // Optimization parameters
    config::SLOPE_PLAIN = 8e-3f;
    config::LM_LAMBDA = 5.0f;
    config::MULTIPASS_BA = 1;
    
    openpano_log(OPENPANO_LOG_DEBUG, "Config applied: mode=%d, focal=%.1f, ordered=%d",
                 cfg->mode, cfg->focal_length, cfg->ordered_input);
}

/* ============================================================================
 * Stitcher lifecycle
 * ============================================================================ */

OPENPANO_API OpenpanoStitcherHandle openpano_stitcher_create(const OpenpanoConfig* config) {
    try {
        auto* stitcher = new OpenpanoStitcher();
        if (config) {
            stitcher->config = *config;
        }
        openpano_log(OPENPANO_LOG_DEBUG, "Stitcher created: %p", stitcher);
        return stitcher;
    } catch (const std::bad_alloc&) {
        openpano_log(OPENPANO_LOG_ERROR, "Failed to allocate stitcher");
        return nullptr;
    } catch (...) {
        openpano_log(OPENPANO_LOG_ERROR, "Unknown error creating stitcher");
        return nullptr;
    }
}

OPENPANO_API void openpano_stitcher_destroy(OpenpanoStitcherHandle handle) {
    if (handle) {
        openpano_log(OPENPANO_LOG_DEBUG, "Stitcher destroyed: %p", handle);
        delete handle;
    }
}

/* ============================================================================
 * Image input
 * ============================================================================ */

OPENPANO_API OpenpanoError openpano_add_image_file(
    OpenpanoStitcherHandle handle,
    const char* file_path
) {
    if (!handle) return OPENPANO_ERROR_INVALID_HANDLE;
    if (!file_path || file_path[0] == '\0') return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    // Verify file exists and is readable
    FILE* f = fopen(file_path, "rb");
    if (!f) {
        openpano_log(OPENPANO_LOG_ERROR, "File not found: %s", file_path);
        return OPENPANO_ERROR_FILE_NOT_FOUND;
    }
    fclose(f);
    
    std::lock_guard<std::mutex> lock(handle->mutex);
    handle->image_paths.push_back(file_path);
    
    openpano_log(OPENPANO_LOG_DEBUG, "Added image file: %s (total: %zu)", 
                 file_path, handle->image_paths.size());
    
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
    if (!pixels) return OPENPANO_ERROR_INVALID_ARGUMENT;
    if (width <= 0 || height <= 0) return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    const int channels = 3;
    if (stride == 0) stride = width * channels;
    
    std::lock_guard<std::mutex> lock(handle->mutex);
    
    try {
        // Create Mat32f and convert uint8 [0,255] to float [0,1]
        auto mat = std::make_unique<Mat32f>(height, width, channels);
        
        for (int y = 0; y < height; ++y) {
            const uint8_t* src_row = pixels + y * stride;
            float* dst_row = mat->ptr(y);
            for (int x = 0; x < width * channels; ++x) {
                dst_row[x] = src_row[x] / 255.0f;
            }
        }
        
        handle->image_data.push_back(std::move(mat));
        
        openpano_log(OPENPANO_LOG_DEBUG, "Added image data: %dx%d (total: %zu)",
                     width, height, handle->image_data.size());
        
        return OPENPANO_OK;
    } catch (const std::bad_alloc&) {
        openpano_log(OPENPANO_LOG_ERROR, "Out of memory adding image");
        return OPENPANO_ERROR_OUT_OF_MEMORY;
    }
}

OPENPANO_API OpenpanoError openpano_add_image_data_float(
    OpenpanoStitcherHandle handle,
    const float* pixels,
    int32_t width,
    int32_t height,
    int32_t stride
) {
    if (!handle) return OPENPANO_ERROR_INVALID_HANDLE;
    if (!pixels) return OPENPANO_ERROR_INVALID_ARGUMENT;
    if (width <= 0 || height <= 0) return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    const int channels = 3;
    if (stride == 0) stride = width * channels * sizeof(float);
    
    std::lock_guard<std::mutex> lock(handle->mutex);
    
    try {
        auto mat = std::make_unique<Mat32f>(height, width, channels);
        
        for (int y = 0; y < height; ++y) {
            const float* src_row = reinterpret_cast<const float*>(
                reinterpret_cast<const uint8_t*>(pixels) + y * stride
            );
            float* dst_row = mat->ptr(y);
            std::memcpy(dst_row, src_row, width * channels * sizeof(float));
        }
        
        handle->image_data.push_back(std::move(mat));
        
        openpano_log(OPENPANO_LOG_DEBUG, "Added float image data: %dx%d", width, height);
        
        return OPENPANO_OK;
    } catch (const std::bad_alloc&) {
        return OPENPANO_ERROR_OUT_OF_MEMORY;
    }
}

OPENPANO_API OpenpanoError openpano_clear_images(OpenpanoStitcherHandle handle) {
    if (!handle) return OPENPANO_ERROR_INVALID_HANDLE;
    
    std::lock_guard<std::mutex> lock(handle->mutex);
    handle->image_paths.clear();
    handle->image_data.clear();
    
    openpano_log(OPENPANO_LOG_DEBUG, "Cleared all images");
    
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
    
    int total_images = openpano_image_count(handle);
    if (total_images < 2) {
        openpano_log(OPENPANO_LOG_ERROR, "Need at least 2 images, got %d", total_images);
        return OPENPANO_ERROR_INSUFFICIENT_IMAGES;
    }
    
    try {
        // Apply configuration to OpenPano globals
        apply_config_to_openpano(&handle->config);
        
        if (callback) callback(0.05f, "Initializing", user_data);
        
        // Build list of image paths (for file-based images)
        // For in-memory images, we'd need to modify OpenPano to support that
        std::vector<std::string> all_paths;
        {
            std::lock_guard<std::mutex> lock(handle->mutex);
            all_paths = handle->image_paths;
        }
        
        // TODO: Handle in-memory images (handle->image_data)
        // This would require extending OpenPano's StitcherBase
        
        if (all_paths.empty()) {
            openpano_log(OPENPANO_LOG_ERROR, "No file-based images to stitch");
            return OPENPANO_ERROR_INSUFFICIENT_IMAGES;
        }
        
        if (callback) callback(0.1f, "Loading images", user_data);
        
        if (handle->cancelled) return OPENPANO_ERROR_CANCELLED;
        
        Mat32f output;
        
        // Choose stitcher based on mode
        if (handle->config.mode == OPENPANO_MODE_CYLINDER) {
            openpano_log(OPENPANO_LOG_INFO, "Using CylinderStitcher");
            if (callback) callback(0.2f, "Cylindrical stitching", user_data);
            
            pano::CylinderStitcher stitcher(std::move(all_paths));
            output = stitcher.build();
        } else {
            openpano_log(OPENPANO_LOG_INFO, "Using Stitcher (camera estimation)");
            if (callback) callback(0.2f, "Feature detection", user_data);
            
            pano::Stitcher stitcher(std::move(all_paths));
            
            if (handle->cancelled) return OPENPANO_ERROR_CANCELLED;
            if (callback) callback(0.5f, "Matching and blending", user_data);
            
            output = stitcher.build();
        }
        
        if (handle->cancelled) return OPENPANO_ERROR_CANCELLED;
        
        // Crop if requested
        if (handle->config.crop_result) {
            if (callback) callback(0.9f, "Cropping", user_data);
            output = pano::crop(output);
        }
        
        if (callback) callback(0.95f, "Finalizing", user_data);
        
        // Wrap result
        auto* img = new OpenpanoImage(std::move(output));
        
        if (callback) callback(1.0f, "Complete", user_data);
        
        *result = img;
        
        openpano_log(OPENPANO_LOG_INFO, "Stitching complete: %dx%d", 
                     img->mat_float.width(), img->mat_float.height());
        
        return OPENPANO_OK;
        
    } catch (const std::bad_alloc&) {
        openpano_log(OPENPANO_LOG_ERROR, "Out of memory during stitching");
        return OPENPANO_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& e) {
        openpano_log(OPENPANO_LOG_ERROR, "Stitching failed: %s", e.what());
        return OPENPANO_ERROR_STITCHING_FAILED;
    } catch (...) {
        openpano_log(OPENPANO_LOG_ERROR, "Unknown error during stitching");
        return OPENPANO_ERROR_STITCHING_FAILED;
    }
}

OPENPANO_API OpenpanoError openpano_cancel(OpenpanoStitcherHandle handle) {
    if (!handle) return OPENPANO_ERROR_INVALID_HANDLE;
    handle->cancelled = true;
    openpano_log(OPENPANO_LOG_INFO, "Cancellation requested");
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
    
    if (width) *width = image->mat_float.width();
    if (height) *height = image->mat_float.height();
    
    return OPENPANO_OK;
}

OPENPANO_API int32_t openpano_image_channels(OpenpanoImageHandle image) {
    if (!image) return -1;
    return image->mat_float.channels();
}

OPENPANO_API OpenpanoError openpano_image_data_float(
    OpenpanoImageHandle image,
    const float** pixels,
    size_t* size
) {
    if (!image) return OPENPANO_ERROR_INVALID_HANDLE;
    if (!pixels || !size) return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    *pixels = image->mat_float.ptr(0);
    *size = image->mat_float.width() * image->mat_float.height() * 
            image->mat_float.channels() * sizeof(float);
    
    return OPENPANO_OK;
}

OPENPANO_API OpenpanoError openpano_image_data(
    OpenpanoImageHandle image,
    const uint8_t** pixels,
    size_t* size
) {
    if (!image) return OPENPANO_ERROR_INVALID_HANDLE;
    if (!pixels || !size) return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    try {
        image->ensure_byte_cache();
        *pixels = image->pixels_byte.data();
        *size = image->pixels_byte.size();
        return OPENPANO_OK;
    } catch (...) {
        return OPENPANO_ERROR_OUT_OF_MEMORY;
    }
}

OPENPANO_API OpenpanoError openpano_image_copy_data(
    OpenpanoImageHandle image,
    uint8_t* buffer,
    size_t buffer_size
) {
    if (!image) return OPENPANO_ERROR_INVALID_HANDLE;
    if (!buffer) return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    try {
        image->ensure_byte_cache();
        size_t copy_size = std::min(buffer_size, image->pixels_byte.size());
        std::memcpy(buffer, image->pixels_byte.data(), copy_size);
        return OPENPANO_OK;
    } catch (...) {
        return OPENPANO_ERROR_OUT_OF_MEMORY;
    }
}

OPENPANO_API OpenpanoError openpano_image_save(
    OpenpanoImageHandle image,
    const char* file_path,
    int32_t quality
) {
    if (!image) return OPENPANO_ERROR_INVALID_HANDLE;
    if (!file_path || file_path[0] == '\0') return OPENPANO_ERROR_INVALID_ARGUMENT;
    
    (void)quality;  // TODO: Implement quality for JPEG
    
    try {
        pano::write_rgb(file_path, image->mat_float);
        openpano_log(OPENPANO_LOG_INFO, "Saved image to: %s", file_path);
        return OPENPANO_OK;
    } catch (const std::exception& e) {
        openpano_log(OPENPANO_LOG_ERROR, "Failed to save image: %s", e.what());
        return OPENPANO_ERROR_STITCHING_FAILED;
    } catch (...) {
        openpano_log(OPENPANO_LOG_ERROR, "Unknown error saving image");
        return OPENPANO_ERROR_UNKNOWN;
    }
}

OPENPANO_API void openpano_image_destroy(OpenpanoImageHandle image) {
    if (image) {
        openpano_log(OPENPANO_LOG_DEBUG, "Image destroyed");
        delete image;
    }
}

/* ============================================================================
 * Utility functions
 * ============================================================================ */

OPENPANO_API const char* openpano_version(void) {
    return OPENPANO_FFI_VERSION;
}

OPENPANO_API int32_t openpano_has_jpeg_support(void) {
#ifdef DISABLE_JPEG
    return 0;
#else
    return 1;
#endif
}

OPENPANO_API int32_t openpano_has_png_support(void) {
    // PNG is always supported via lodepng
    return 1;
}

OPENPANO_API int32_t openpano_get_cpu_count(void) {
    unsigned int count = std::thread::hardware_concurrency();
    return (count > 0) ? static_cast<int32_t>(count) : 1;
}

/* ============================================================================
 * Logging
 * ============================================================================ */

OPENPANO_API void openpano_set_log_level(OpenpanoLogLevel level) {
    g_log_level = level;
}

OPENPANO_API void openpano_set_log_callback(
    OpenpanoLogCallback callback,
    void* user_data
) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_log_callback = callback;
    g_log_user_data = user_data;
}

