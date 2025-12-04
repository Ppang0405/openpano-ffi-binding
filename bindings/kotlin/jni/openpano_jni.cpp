/**
 * @file openpano_jni.cpp
 * @brief JNI bridge implementation for OpenPano Android bindings
 *
 * This file implements the native methods declared in OpenPanoJNI.kt.
 */

#include <jni.h>
#include <string>
#include <cstring>
#include "openpano_ffi.h"

#define JNI_METHOD(return_type, name) \
    JNIEXPORT return_type JNICALL Java_com_openpano_OpenPanoJNI_##name

extern "C" {

/* ============================================================================
 * Stitcher lifecycle
 * ============================================================================ */

/**
 * Create a new stitcher with configuration parameters.
 */
JNI_METHOD(jlong, nativeCreateStitcher)(
    JNIEnv* env,
    jobject /* this */,
    jint mode,
    jfloat focalLength,
    jboolean orderedInput,
    jboolean cropResult,
    jint numThreads,
    jboolean lazyRead,
    jboolean straighten,
    jint maxOutputSize,
    jint multiband
) {
    OpenpanoConfig config;
    openpano_config_default(&config);
    
    config.mode = static_cast<OpenpanoMode>(mode);
    config.focal_length = focalLength;
    config.ordered_input = orderedInput ? 1 : 0;
    config.crop_result = cropResult ? 1 : 0;
    config.num_threads = numThreads;
    config.lazy_read = lazyRead ? 1 : 0;
    config.straighten = straighten ? 1 : 0;
    config.max_output_size = maxOutputSize;
    config.multiband = multiband;
    
    OpenpanoStitcherHandle handle = openpano_stitcher_create(&config);
    return reinterpret_cast<jlong>(handle);
}

/**
 * Destroy a stitcher and release resources.
 */
JNI_METHOD(void, nativeDestroyStitcher)(
    JNIEnv* env,
    jobject /* this */,
    jlong handle
) {
    if (handle != 0) {
        openpano_stitcher_destroy(reinterpret_cast<OpenpanoStitcherHandle>(handle));
    }
}

/* ============================================================================
 * Image input
 * ============================================================================ */

/**
 * Add an image file to the stitcher.
 */
JNI_METHOD(jint, nativeAddImageFile)(
    JNIEnv* env,
    jobject /* this */,
    jlong handle,
    jstring path
) {
    if (handle == 0 || path == nullptr) {
        return OPENPANO_ERROR_INVALID_ARGUMENT;
    }
    
    const char* pathStr = env->GetStringUTFChars(path, nullptr);
    if (pathStr == nullptr) {
        return OPENPANO_ERROR_OUT_OF_MEMORY;
    }
    
    OpenpanoError result = openpano_add_image_file(
        reinterpret_cast<OpenpanoStitcherHandle>(handle),
        pathStr
    );
    
    env->ReleaseStringUTFChars(path, pathStr);
    return static_cast<jint>(result);
}

/**
 * Add an image from raw pixel data.
 */
JNI_METHOD(jint, nativeAddImageData)(
    JNIEnv* env,
    jobject /* this */,
    jlong handle,
    jbyteArray pixels,
    jint width,
    jint height
) {
    if (handle == 0 || pixels == nullptr) {
        return OPENPANO_ERROR_INVALID_ARGUMENT;
    }
    
    jbyte* pixelData = env->GetByteArrayElements(pixels, nullptr);
    if (pixelData == nullptr) {
        return OPENPANO_ERROR_OUT_OF_MEMORY;
    }
    
    OpenpanoError result = openpano_add_image_data(
        reinterpret_cast<OpenpanoStitcherHandle>(handle),
        reinterpret_cast<const uint8_t*>(pixelData),
        width,
        height,
        0  // auto stride
    );
    
    env->ReleaseByteArrayElements(pixels, pixelData, JNI_ABORT);
    return static_cast<jint>(result);
}

/**
 * Get number of images added.
 */
JNI_METHOD(jint, nativeImageCount)(
    JNIEnv* env,
    jobject /* this */,
    jlong handle
) {
    if (handle == 0) return -1;
    return openpano_image_count(reinterpret_cast<OpenpanoStitcherHandle>(handle));
}

/**
 * Clear all images.
 */
JNI_METHOD(jint, nativeClearImages)(
    JNIEnv* env,
    jobject /* this */,
    jlong handle
) {
    if (handle == 0) return OPENPANO_ERROR_INVALID_HANDLE;
    return openpano_clear_images(reinterpret_cast<OpenpanoStitcherHandle>(handle));
}

/* ============================================================================
 * Stitching execution
 * ============================================================================ */

/**
 * Execute stitching.
 */
JNI_METHOD(jlong, nativeStitch)(
    JNIEnv* env,
    jobject /* this */,
    jlong handle
) {
    if (handle == 0) return 0;
    
    OpenpanoImageHandle result = nullptr;
    OpenpanoError err = openpano_stitch(
        reinterpret_cast<OpenpanoStitcherHandle>(handle),
        &result
    );
    
    if (err != OPENPANO_OK) {
        return 0;
    }
    
    return reinterpret_cast<jlong>(result);
}

/**
 * Progress callback context for JNI.
 */
struct JNIProgressContext {
    JNIEnv* env;
    jobject callback;
    jmethodID methodId;
};

/**
 * C callback that calls the Java callback.
 */
static void jniProgressCallback(float progress, const char* stage, void* userData) {
    auto* ctx = static_cast<JNIProgressContext*>(userData);
    if (ctx == nullptr || ctx->callback == nullptr) return;
    
    jstring jStage = ctx->env->NewStringUTF(stage ? stage : "");
    ctx->env->CallVoidMethod(ctx->callback, ctx->methodId, progress, jStage);
    ctx->env->DeleteLocalRef(jStage);
}

/**
 * Execute stitching with progress callback.
 */
JNI_METHOD(jlong, nativeStitchWithProgress)(
    JNIEnv* env,
    jobject /* this */,
    jlong handle,
    jobject callback
) {
    if (handle == 0) return 0;
    
    JNIProgressContext ctx = {nullptr, nullptr, nullptr};
    
    if (callback != nullptr) {
        ctx.env = env;
        ctx.callback = callback;
        
        jclass callbackClass = env->GetObjectClass(callback);
        ctx.methodId = env->GetMethodID(
            callbackClass, 
            "onProgress", 
            "(FLjava/lang/String;)V"
        );
        env->DeleteLocalRef(callbackClass);
    }
    
    OpenpanoImageHandle result = nullptr;
    OpenpanoError err = openpano_stitch_with_progress(
        reinterpret_cast<OpenpanoStitcherHandle>(handle),
        &result,
        callback ? jniProgressCallback : nullptr,
        callback ? &ctx : nullptr
    );
    
    if (err != OPENPANO_OK) {
        return 0;
    }
    
    return reinterpret_cast<jlong>(result);
}

/**
 * Cancel an ongoing stitching operation.
 */
JNI_METHOD(jint, nativeCancel)(
    JNIEnv* env,
    jobject /* this */,
    jlong handle
) {
    if (handle == 0) return OPENPANO_ERROR_INVALID_HANDLE;
    return openpano_cancel(reinterpret_cast<OpenpanoStitcherHandle>(handle));
}

/* ============================================================================
 * Image handling
 * ============================================================================ */

/**
 * Destroy an image and release resources.
 */
JNI_METHOD(void, nativeDestroyImage)(
    JNIEnv* env,
    jobject /* this */,
    jlong handle
) {
    if (handle != 0) {
        openpano_image_destroy(reinterpret_cast<OpenpanoImageHandle>(handle));
    }
}

/**
 * Get image dimensions.
 */
JNI_METHOD(jintArray, nativeImageDimensions)(
    JNIEnv* env,
    jobject /* this */,
    jlong handle
) {
    jintArray result = env->NewIntArray(2);
    if (result == nullptr) return nullptr;
    
    int32_t width = 0, height = 0;
    if (handle != 0) {
        openpano_image_dimensions(
            reinterpret_cast<OpenpanoImageHandle>(handle),
            &width,
            &height
        );
    }
    
    jint dims[2] = {width, height};
    env->SetIntArrayRegion(result, 0, 2, dims);
    return result;
}

/**
 * Get number of channels.
 */
JNI_METHOD(jint, nativeImageChannels)(
    JNIEnv* env,
    jobject /* this */,
    jlong handle
) {
    if (handle == 0) return -1;
    return openpano_image_channels(reinterpret_cast<OpenpanoImageHandle>(handle));
}

/**
 * Get pixel data as byte array.
 */
JNI_METHOD(jbyteArray, nativeImageData)(
    JNIEnv* env,
    jobject /* this */,
    jlong handle
) {
    if (handle == 0) return nullptr;
    
    const uint8_t* pixels = nullptr;
    size_t size = 0;
    
    OpenpanoError err = openpano_image_data(
        reinterpret_cast<OpenpanoImageHandle>(handle),
        &pixels,
        &size
    );
    
    if (err != OPENPANO_OK || pixels == nullptr || size == 0) {
        return nullptr;
    }
    
    jbyteArray result = env->NewByteArray(static_cast<jsize>(size));
    if (result == nullptr) return nullptr;
    
    env->SetByteArrayRegion(
        result, 
        0, 
        static_cast<jsize>(size), 
        reinterpret_cast<const jbyte*>(pixels)
    );
    
    return result;
}

/**
 * Save image to file.
 */
JNI_METHOD(jint, nativeImageSave)(
    JNIEnv* env,
    jobject /* this */,
    jlong handle,
    jstring path,
    jint quality
) {
    if (handle == 0 || path == nullptr) {
        return OPENPANO_ERROR_INVALID_ARGUMENT;
    }
    
    const char* pathStr = env->GetStringUTFChars(path, nullptr);
    if (pathStr == nullptr) {
        return OPENPANO_ERROR_OUT_OF_MEMORY;
    }
    
    OpenpanoError result = openpano_image_save(
        reinterpret_cast<OpenpanoImageHandle>(handle),
        pathStr,
        quality
    );
    
    env->ReleaseStringUTFChars(path, pathStr);
    return static_cast<jint>(result);
}

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/**
 * Get library version.
 */
JNI_METHOD(jstring, nativeVersion)(
    JNIEnv* env,
    jobject /* this */
) {
    return env->NewStringUTF(openpano_version());
}

/**
 * Check JPEG support.
 */
JNI_METHOD(jboolean, nativeHasJpegSupport)(
    JNIEnv* env,
    jobject /* this */
) {
    return openpano_has_jpeg_support() != 0;
}

/**
 * Check PNG support.
 */
JNI_METHOD(jboolean, nativeHasPngSupport)(
    JNIEnv* env,
    jobject /* this */
) {
    return openpano_has_png_support() != 0;
}

/**
 * Get CPU count.
 */
JNI_METHOD(jint, nativeGetCpuCount)(
    JNIEnv* env,
    jobject /* this */
) {
    return openpano_get_cpu_count();
}

/**
 * Set log level.
 */
JNI_METHOD(void, nativeSetLogLevel)(
    JNIEnv* env,
    jobject /* this */,
    jint level
) {
    openpano_set_log_level(static_cast<OpenpanoLogLevel>(level));
}

} // extern "C"

