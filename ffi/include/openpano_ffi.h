/**
 * @file openpano_ffi.h
 * @brief C-compatible FFI bindings for OpenPano panorama stitching library
 * 
 * This header provides a portable C interface that can be called from
 * Rust, Go, Swift, Kotlin, and other languages via FFI.
 * 
 * @section Usage Example
 * @code
 * OpenpanoConfig config;
 * openpano_config_default(&config);
 * config.mode = OPENPANO_MODE_ESTIMATE_CAMERA;
 * 
 * OpenpanoStitcherHandle stitcher = openpano_stitcher_create(&config);
 * openpano_add_image_file(stitcher, "image1.jpg");
 * openpano_add_image_file(stitcher, "image2.jpg");
 * 
 * OpenpanoImageHandle result;
 * if (openpano_stitch(stitcher, &result) == OPENPANO_OK) {
 *     openpano_image_save(result, "panorama.jpg", 95);
 *     openpano_image_destroy(result);
 * }
 * openpano_stitcher_destroy(stitcher);
 * @endcode
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

/**
 * @brief Opaque handle to a stitcher instance
 * 
 * This handle manages the lifecycle of a panorama stitching session.
 * Create with openpano_stitcher_create(), destroy with openpano_stitcher_destroy().
 */
typedef struct OpenpanoStitcher* OpenpanoStitcherHandle;

/**
 * @brief Opaque handle to a result image
 * 
 * This handle references the output panorama image.
 * Destroy with openpano_image_destroy() when done.
 */
typedef struct OpenpanoImage* OpenpanoImageHandle;

/* ============================================================================
 * Error handling
 * ============================================================================ */

/**
 * @brief Error codes returned by OpenPano functions
 */
typedef enum OpenpanoError {
    /** Operation completed successfully */
    OPENPANO_OK = 0,
    /** Invalid or null handle provided */
    OPENPANO_ERROR_INVALID_HANDLE = -1,
    /** Invalid argument or parameter value */
    OPENPANO_ERROR_INVALID_ARGUMENT = -2,
    /** Specified file could not be found or opened */
    OPENPANO_ERROR_FILE_NOT_FOUND = -3,
    /** Need at least 2 images for stitching */
    OPENPANO_ERROR_INSUFFICIENT_IMAGES = -4,
    /** Stitching operation failed */
    OPENPANO_ERROR_STITCHING_FAILED = -5,
    /** Memory allocation failed */
    OPENPANO_ERROR_OUT_OF_MEMORY = -6,
    /** Feature detection failed on one or more images */
    OPENPANO_ERROR_FEATURE_DETECTION_FAILED = -7,
    /** Feature matching between images failed */
    OPENPANO_ERROR_MATCHING_FAILED = -8,
    /** Image format not supported */
    OPENPANO_ERROR_UNSUPPORTED_FORMAT = -9,
    /** Operation was cancelled */
    OPENPANO_ERROR_CANCELLED = -10,
    /** Unknown or unspecified error */
    OPENPANO_ERROR_UNKNOWN = -99
} OpenpanoError;

/**
 * @brief Get human-readable error message for an error code
 * 
 * @param error The error code to describe
 * @return Static string describing the error (do not free)
 */
OPENPANO_API const char* openpano_error_string(OpenpanoError error);

/* ============================================================================
 * Stitching mode configuration
 * ============================================================================ */

/**
 * @brief Stitching modes supported by OpenPano
 */
typedef enum OpenpanoMode {
    /** 
     * Cylinder projection mode
     * Best for: Camera rotating around vertical axis (standard panoramas)
     * Requires: ORDERED_INPUT, known FOCAL_LENGTH
     */
    OPENPANO_MODE_CYLINDER = 0,
    
    /** 
     * Camera estimation mode with bundle adjustment
     * Best for: Arbitrary camera poses, unordered images
     * Most flexible but computationally intensive
     */
    OPENPANO_MODE_ESTIMATE_CAMERA = 1,
    
    /** 
     * Translation mode
     * Best for: Pure translation between images (document scanning, mosaics)
     * Requires: ORDERED_INPUT
     */
    OPENPANO_MODE_TRANSLATION = 2
} OpenpanoMode;

/**
 * @brief Configuration options for stitching
 */
typedef struct OpenpanoConfig {
    /** Stitching mode (see OpenpanoMode) */
    OpenpanoMode mode;
    
    /** 
     * Focal length in 35mm equivalent (used in CYLINDER mode)
     * Typical values: 24-70mm for standard lenses
     */
    float focal_length;
    
    /** 
     * Whether input images are in sequential order
     * 1 = ordered (left-to-right or right-to-left)
     * 0 = unordered (will auto-detect connections)
     */
    int32_t ordered_input;
    
    /** 
     * Whether to crop result to remove black borders
     * 1 = crop to largest inner rectangle
     * 0 = keep full output with potential black areas
     */
    int32_t crop_result;
    
    /** 
     * Number of threads to use
     * 0 = auto-detect based on CPU cores
     */
    int32_t num_threads;
    
    /** 
     * Enable lazy image loading to reduce peak memory usage
     * 1 = load images on-demand
     * 0 = load all images upfront
     */
    int32_t lazy_read;
    
    /**
     * Whether to straighten the output panorama
     * 1 = apply perspective correction
     * 0 = keep natural curvature
     */
    int32_t straighten;
    
    /**
     * Maximum output image dimension (width or height)
     * 0 = no limit
     */
    int32_t max_output_size;
    
    /** 
     * Use multiband blending for smoother seams
     * 0 = disable, 1+ = number of bands
     */
    int32_t multiband;
    
    /* Reserved for future use */
    int32_t reserved[8];
} OpenpanoConfig;

/**
 * @brief Get default configuration values
 * 
 * Initializes config struct with sensible defaults:
 * - mode: ESTIMATE_CAMERA
 * - focal_length: 36.0 (35mm equiv)
 * - ordered_input: 0 (auto-detect)
 * - crop_result: 1 (enabled)
 * - num_threads: 0 (auto)
 * 
 * @param config Pointer to config struct to fill with defaults
 */
OPENPANO_API void openpano_config_default(OpenpanoConfig* config);

/* ============================================================================
 * Stitcher lifecycle
 * ============================================================================ */

/**
 * @brief Create a new stitcher instance
 * 
 * @param config Configuration options (NULL for defaults)
 * @return Handle to stitcher, or NULL on failure (check errno/GetLastError)
 */
OPENPANO_API OpenpanoStitcherHandle openpano_stitcher_create(
    const OpenpanoConfig* config
);

/**
 * @brief Destroy a stitcher instance and free all resources
 * 
 * This will also release any images added to the stitcher.
 * Safe to pass NULL (no-op).
 * 
 * @param handle Stitcher handle to destroy
 */
OPENPANO_API void openpano_stitcher_destroy(OpenpanoStitcherHandle handle);

/* ============================================================================
 * Image input methods
 * ============================================================================ */

/**
 * @brief Add an image file to the stitcher
 * 
 * Supported formats: JPEG, PNG (depending on build configuration)
 * 
 * @param handle Stitcher handle
 * @param file_path Path to image file (UTF-8 encoded)
 * @return OPENPANO_OK on success, error code otherwise
 */
OPENPANO_API OpenpanoError openpano_add_image_file(
    OpenpanoStitcherHandle handle,
    const char* file_path
);

/**
 * @brief Add an image from raw pixel data
 * 
 * @param handle Stitcher handle
 * @param pixels Pointer to pixel data (RGB, 8-bit per channel, row-major)
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param stride Bytes per row (0 = width * 3 for tightly packed RGB)
 * @return OPENPANO_OK on success, error code otherwise
 * 
 * @note The pixel data is copied, so the caller can free it after this call
 */
OPENPANO_API OpenpanoError openpano_add_image_data(
    OpenpanoStitcherHandle handle,
    const uint8_t* pixels,
    int32_t width,
    int32_t height,
    int32_t stride
);

/**
 * @brief Add an image from raw pixel data (float format)
 * 
 * @param handle Stitcher handle
 * @param pixels Pointer to pixel data (RGB, float per channel in [0,1], row-major)
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param stride Bytes per row (0 = width * 3 * sizeof(float))
 * @return OPENPANO_OK on success, error code otherwise
 */
OPENPANO_API OpenpanoError openpano_add_image_data_float(
    OpenpanoStitcherHandle handle,
    const float* pixels,
    int32_t width,
    int32_t height,
    int32_t stride
);

/**
 * @brief Clear all added images
 * 
 * @param handle Stitcher handle
 * @return OPENPANO_OK on success, error code otherwise
 */
OPENPANO_API OpenpanoError openpano_clear_images(OpenpanoStitcherHandle handle);

/**
 * @brief Get number of images currently added
 * 
 * @param handle Stitcher handle
 * @return Number of images, or -1 on error
 */
OPENPANO_API int32_t openpano_image_count(OpenpanoStitcherHandle handle);

/* ============================================================================
 * Stitching execution
 * ============================================================================ */

/**
 * @brief Progress callback function type
 * 
 * @param progress Current progress from 0.0 to 1.0
 * @param stage Human-readable description of current stage
 * @param user_data User-provided context from openpano_stitch_with_progress
 */
typedef void (*OpenpanoProgressCallback)(
    float progress,
    const char* stage,
    void* user_data
);

/**
 * @brief Execute panorama stitching
 * 
 * This is a blocking call that may take significant time depending on
 * image count and size.
 * 
 * @param handle Stitcher handle
 * @param result Output handle for result image (must not be NULL)
 * @return OPENPANO_OK on success, error code otherwise
 */
OPENPANO_API OpenpanoError openpano_stitch(
    OpenpanoStitcherHandle handle,
    OpenpanoImageHandle* result
);

/**
 * @brief Execute panorama stitching with progress callback
 * 
 * @param handle Stitcher handle
 * @param result Output handle for result image
 * @param callback Progress callback function (NULL to disable)
 * @param user_data Context passed to callback
 * @return OPENPANO_OK on success, error code otherwise
 */
OPENPANO_API OpenpanoError openpano_stitch_with_progress(
    OpenpanoStitcherHandle handle,
    OpenpanoImageHandle* result,
    OpenpanoProgressCallback callback,
    void* user_data
);

/**
 * @brief Cancel an ongoing stitching operation
 * 
 * Can be called from another thread to abort a running stitch.
 * 
 * @param handle Stitcher handle
 * @return OPENPANO_OK on success, error code otherwise
 */
OPENPANO_API OpenpanoError openpano_cancel(OpenpanoStitcherHandle handle);

/* ============================================================================
 * Result image handling
 * ============================================================================ */

/**
 * @brief Get result image dimensions
 * 
 * @param image Image handle
 * @param width Output width in pixels (can be NULL)
 * @param height Output height in pixels (can be NULL)
 * @return OPENPANO_OK on success, error code otherwise
 */
OPENPANO_API OpenpanoError openpano_image_dimensions(
    OpenpanoImageHandle image,
    int32_t* width,
    int32_t* height
);

/**
 * @brief Get number of channels in the image
 * 
 * @param image Image handle
 * @return Number of channels (typically 3 for RGB), or -1 on error
 */
OPENPANO_API int32_t openpano_image_channels(OpenpanoImageHandle image);

/**
 * @brief Get pointer to result image pixel data (float format)
 * 
 * The returned pointer is valid until the image is destroyed.
 * Data format: RGB float [0,1] range, row-major order.
 * 
 * @param image Image handle
 * @param pixels Output pointer to pixel data
 * @param size Output size of pixel data in bytes
 * @return OPENPANO_OK on success, error code otherwise
 */
OPENPANO_API OpenpanoError openpano_image_data_float(
    OpenpanoImageHandle image,
    const float** pixels,
    size_t* size
);

/**
 * @brief Get pointer to result image pixel data (8-bit format)
 * 
 * @param image Image handle
 * @param pixels Output pointer to pixel data (RGB, 8-bit per channel)
 * @param size Output size of pixel data in bytes
 * @return OPENPANO_OK on success, error code otherwise
 * 
 * @note This may trigger an internal conversion from float to uint8
 */
OPENPANO_API OpenpanoError openpano_image_data(
    OpenpanoImageHandle image,
    const uint8_t** pixels,
    size_t* size
);

/**
 * @brief Copy result image pixel data to provided buffer (8-bit format)
 * 
 * @param image Image handle
 * @param buffer Destination buffer (must be pre-allocated)
 * @param buffer_size Size of destination buffer in bytes
 * @return OPENPANO_OK on success, error code otherwise
 */
OPENPANO_API OpenpanoError openpano_image_copy_data(
    OpenpanoImageHandle image,
    uint8_t* buffer,
    size_t buffer_size
);

/**
 * @brief Save result image to file
 * 
 * Format is determined by file extension:
 * - .jpg, .jpeg: JPEG format (if supported)
 * - .png: PNG format (always supported via lodepng)
 * 
 * @param image Image handle
 * @param file_path Output file path (UTF-8)
 * @param quality JPEG quality 1-100 (ignored for PNG)
 * @return OPENPANO_OK on success, error code otherwise
 */
OPENPANO_API OpenpanoError openpano_image_save(
    OpenpanoImageHandle image,
    const char* file_path,
    int32_t quality
);

/**
 * @brief Destroy result image and free memory
 * 
 * Safe to pass NULL (no-op).
 * 
 * @param image Image handle to destroy
 */
OPENPANO_API void openpano_image_destroy(OpenpanoImageHandle image);

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/**
 * @brief Get OpenPano library version string
 * 
 * @return Version string (e.g., "1.0.0") - do not free
 */
OPENPANO_API const char* openpano_version(void);

/**
 * @brief Check if library was built with JPEG support
 * 
 * @return 1 if JPEG format is supported, 0 otherwise
 */
OPENPANO_API int32_t openpano_has_jpeg_support(void);

/**
 * @brief Check if library was built with PNG support
 * 
 * @return 1 if PNG format is supported, 0 otherwise
 */
OPENPANO_API int32_t openpano_has_png_support(void);

/**
 * @brief Get number of CPU cores available
 * 
 * @return Number of logical CPU cores
 */
OPENPANO_API int32_t openpano_get_cpu_count(void);

/* ============================================================================
 * Logging
 * ============================================================================ */

/**
 * @brief Log levels for openpano_set_log_level
 */
typedef enum OpenpanoLogLevel {
    OPENPANO_LOG_NONE = 0,   /**< No logging */
    OPENPANO_LOG_ERROR = 1,  /**< Errors only */
    OPENPANO_LOG_WARN = 2,   /**< Warnings and errors */
    OPENPANO_LOG_INFO = 3,   /**< Info, warnings, and errors */
    OPENPANO_LOG_DEBUG = 4   /**< All messages including debug */
} OpenpanoLogLevel;

/**
 * @brief Set global log level
 * 
 * @param level Log level (see OpenpanoLogLevel)
 */
OPENPANO_API void openpano_set_log_level(OpenpanoLogLevel level);

/**
 * @brief Log callback function type
 * 
 * @param level Log level of the message
 * @param message The log message (UTF-8)
 * @param user_data User-provided context
 */
typedef void (*OpenpanoLogCallback)(
    OpenpanoLogLevel level,
    const char* message,
    void* user_data
);

/**
 * @brief Set custom log callback
 * 
 * When set, all log messages will be sent to this callback instead of
 * being printed to stderr.
 * 
 * @param callback Log callback function (NULL to disable and use stderr)
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

