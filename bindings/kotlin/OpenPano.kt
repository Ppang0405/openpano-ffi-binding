/**
 * OpenPano - Kotlin/JNI Bindings for Panorama Stitching
 *
 * This module provides Kotlin bindings to the OpenPano C++ library for
 * creating panoramic images from multiple overlapping photographs.
 *
 * Example usage:
 * ```kotlin
 * val config = StitcherConfig(
 *     mode = StitchMode.ESTIMATE_CAMERA,
 *     cropResult = true
 * )
 *
 * Stitcher(config).use { stitcher ->
 *     stitcher.addImage("/path/to/photo1.jpg")
 *     stitcher.addImage("/path/to/photo2.jpg")
 *
 *     stitcher.stitch().use { panorama ->
 *         panorama.save("/path/to/output.jpg", quality = 90)
 *         // Or convert to Bitmap
 *         val bitmap = panorama.toBitmap()
 *     }
 * }
 * ```
 *
 * @author OpenPano Contributors
 */
package com.openpano

import android.graphics.Bitmap
import java.io.Closeable
import java.nio.ByteBuffer

/**
 * JNI bridge to OpenPano native library.
 *
 * This object handles loading the native library and declares all JNI methods.
 * Users should not call these methods directly - use the high-level Kotlin API instead.
 */
object OpenPanoJNI {
    init {
        System.loadLibrary("openpano_jni")
    }

    // Stitcher lifecycle
    external fun nativeCreateStitcher(
        mode: Int,
        focalLength: Float,
        orderedInput: Boolean,
        cropResult: Boolean,
        numThreads: Int,
        lazyRead: Boolean,
        straighten: Boolean,
        maxOutputSize: Int,
        multiband: Int
    ): Long

    external fun nativeDestroyStitcher(handle: Long)

    // Image input
    external fun nativeAddImageFile(handle: Long, path: String): Int
    external fun nativeAddImageData(handle: Long, pixels: ByteArray, width: Int, height: Int): Int
    external fun nativeImageCount(handle: Long): Int
    external fun nativeClearImages(handle: Long): Int

    // Stitching
    external fun nativeStitch(handle: Long): Long
    external fun nativeStitchWithProgress(
        handle: Long,
        callback: ProgressCallback?
    ): Long
    external fun nativeCancel(handle: Long): Int

    // Image handling
    external fun nativeDestroyImage(handle: Long)
    external fun nativeImageDimensions(handle: Long): IntArray
    external fun nativeImageChannels(handle: Long): Int
    external fun nativeImageData(handle: Long): ByteArray?
    external fun nativeImageSave(handle: Long, path: String, quality: Int): Int

    // Utility
    external fun nativeVersion(): String
    external fun nativeHasJpegSupport(): Boolean
    external fun nativeHasPngSupport(): Boolean
    external fun nativeGetCpuCount(): Int
    external fun nativeSetLogLevel(level: Int)

    /**
     * Progress callback interface for JNI.
     */
    interface ProgressCallback {
        /**
         * Called during stitching to report progress.
         *
         * @param progress Progress value from 0.0 to 1.0
         * @param stage Description of current operation
         */
        fun onProgress(progress: Float, stage: String)
    }
}

/**
 * Stitching algorithm modes.
 */
enum class StitchMode(val value: Int) {
    /**
     * Cylindrical projection mode.
     * Best for standard panoramas where camera rotates around vertical axis.
     * Requires ordered input and known focal length.
     */
    CYLINDER(0),

    /**
     * Camera estimation mode with bundle adjustment.
     * Most flexible - works with arbitrary camera poses and unordered images.
     */
    ESTIMATE_CAMERA(1),

    /**
     * Translation mode.
     * Best for pure translation (document scanning, satellite imagery).
     * Requires ordered input.
     */
    TRANSLATION(2)
}

/**
 * Log verbosity levels.
 */
enum class LogLevel(val value: Int) {
    /** No logging output */
    NONE(0),
    /** Only error messages */
    ERROR(1),
    /** Warnings and errors */
    WARN(2),
    /** Informational messages, warnings, and errors */
    INFO(3),
    /** All messages including debug output */
    DEBUG(4)
}

/**
 * Configuration options for panorama stitching.
 *
 * @property mode Stitching algorithm mode
 * @property focalLength Focal length in 35mm equivalent (for cylinder mode)
 * @property orderedInput Whether images are in sequential order
 * @property cropResult Whether to crop result to remove black borders
 * @property numThreads Number of threads (0 = auto-detect)
 * @property lazyRead Enable lazy image loading to reduce memory
 * @property straighten Enable perspective straightening
 * @property maxOutputSize Maximum output dimension (0 = no limit)
 * @property multiband Number of bands for multi-band blending (0 = disabled)
 */
data class StitcherConfig(
    val mode: StitchMode = StitchMode.ESTIMATE_CAMERA,
    val focalLength: Float = 36f,
    val orderedInput: Boolean = false,
    val cropResult: Boolean = true,
    val numThreads: Int = 0,
    val lazyRead: Boolean = false,
    val straighten: Boolean = false,
    val maxOutputSize: Int = 0,
    val multiband: Int = 1
)

/**
 * Exceptions thrown by OpenPano operations.
 */
sealed class OpenPanoException(message: String) : Exception(message) {
    /** The provided handle is invalid or null */
    object InvalidHandle : OpenPanoException("Invalid handle")

    /** An invalid argument was provided */
    class InvalidArgument(detail: String = "") : 
        OpenPanoException("Invalid argument${if (detail.isNotEmpty()) ": $detail" else ""}")

    /** The specified file was not found */
    class FileNotFound(path: String = "") : 
        OpenPanoException("File not found${if (path.isNotEmpty()) ": $path" else ""}")

    /** At least 2 images are required for stitching */
    object InsufficientImages : OpenPanoException("Need at least 2 images")

    /** The stitching operation failed */
    class StitchingFailed(detail: String = "") : 
        OpenPanoException("Stitching failed${if (detail.isNotEmpty()) ": $detail" else ""}")

    /** Memory allocation failed */
    object OutOfMemory : OpenPanoException("Out of memory")

    /** Feature detection failed on one or more images */
    object FeatureDetectionFailed : OpenPanoException("Feature detection failed")

    /** Feature matching between images failed */
    object MatchingFailed : OpenPanoException("Feature matching failed")

    /** The image format is not supported */
    object UnsupportedFormat : OpenPanoException("Unsupported format")

    /** The operation was cancelled */
    object Cancelled : OpenPanoException("Operation cancelled")

    /** An unknown error occurred */
    class Unknown(code: Int) : OpenPanoException("Unknown error: $code")

    companion object {
        /**
         * Create an exception from a native error code.
         */
        fun fromCode(code: Int): OpenPanoException = when (code) {
            -1 -> InvalidHandle
            -2 -> InvalidArgument()
            -3 -> FileNotFound()
            -4 -> InsufficientImages
            -5 -> StitchingFailed()
            -6 -> OutOfMemory
            -7 -> FeatureDetectionFailed
            -8 -> MatchingFailed
            -9 -> UnsupportedFormat
            -10 -> Cancelled
            else -> Unknown(code)
        }
    }
}

/**
 * Result panorama image.
 *
 * This class wraps the native image handle and provides methods to access
 * pixel data, convert to Bitmap, or save to file.
 *
 * Always call [close] when done to release native resources.
 */
class PanoramaImage internal constructor(private var handle: Long) : Closeable {

    /**
     * Image dimensions as [width, height].
     */
    val dimensions: Pair<Int, Int>
        get() {
            check(handle != 0L) { "Image already closed" }
            val dims = OpenPanoJNI.nativeImageDimensions(handle)
            return Pair(dims[0], dims[1])
        }

    /**
     * Image width in pixels.
     */
    val width: Int get() = dimensions.first

    /**
     * Image height in pixels.
     */
    val height: Int get() = dimensions.second

    /**
     * Number of color channels (typically 3 for RGB).
     */
    val channels: Int
        get() {
            check(handle != 0L) { "Image already closed" }
            return OpenPanoJNI.nativeImageChannels(handle)
        }

    /**
     * Get raw pixel data as RGB bytes.
     *
     * @return ByteArray containing RGB pixel data in row-major order
     * @throws OpenPanoException if the operation fails
     */
    fun pixels(): ByteArray {
        check(handle != 0L) { "Image already closed" }
        return OpenPanoJNI.nativeImageData(handle)
            ?: throw OpenPanoException.OutOfMemory
    }

    /**
     * Convert to Android Bitmap.
     *
     * @return ARGB_8888 Bitmap, or null if conversion fails
     */
    fun toBitmap(): Bitmap? {
        if (handle == 0L) return null

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

    /**
     * Save image to file.
     *
     * Format is determined by file extension:
     * - .jpg, .jpeg: JPEG format
     * - .png: PNG format
     *
     * @param path Output file path
     * @param quality JPEG quality (1-100, ignored for PNG)
     * @throws OpenPanoException if the operation fails
     */
    fun save(path: String, quality: Int = 90) {
        check(handle != 0L) { "Image already closed" }
        val err = OpenPanoJNI.nativeImageSave(handle, path, quality)
        if (err != 0) throw OpenPanoException.fromCode(err)
    }

    /**
     * Release native resources.
     *
     * Safe to call multiple times.
     */
    override fun close() {
        if (handle != 0L) {
            OpenPanoJNI.nativeDestroyImage(handle)
            handle = 0L
        }
    }
}

/**
 * Panorama stitcher for combining multiple images.
 *
 * Example:
 * ```kotlin
 * Stitcher().use { stitcher ->
 *     stitcher.addImage("photo1.jpg")
 *     stitcher.addImage("photo2.jpg")
 *     val panorama = stitcher.stitch()
 *     panorama.save("output.jpg")
 * }
 * ```
 *
 * @param config Configuration options for stitching
 */
class Stitcher(config: StitcherConfig = StitcherConfig()) : Closeable {

    private var handle: Long

    init {
        handle = OpenPanoJNI.nativeCreateStitcher(
            mode = config.mode.value,
            focalLength = config.focalLength,
            orderedInput = config.orderedInput,
            cropResult = config.cropResult,
            numThreads = config.numThreads,
            lazyRead = config.lazyRead,
            straighten = config.straighten,
            maxOutputSize = config.maxOutputSize,
            multiband = config.multiband
        )
        if (handle == 0L) throw OpenPanoException.OutOfMemory
    }

    /**
     * Add an image file to the stitcher.
     *
     * @param path Path to image file
     * @throws OpenPanoException.FileNotFound if file doesn't exist
     */
    fun addImage(path: String) {
        check(handle != 0L) { "Stitcher already closed" }
        val err = OpenPanoJNI.nativeAddImageFile(handle, path)
        if (err != 0) {
            if (err == -3) throw OpenPanoException.FileNotFound(path)
            throw OpenPanoException.fromCode(err)
        }
    }

    /**
     * Add an image from Android Bitmap.
     *
     * @param bitmap Bitmap to add (must be ARGB_8888)
     * @throws OpenPanoException if the operation fails
     */
    fun addImage(bitmap: Bitmap) {
        check(handle != 0L) { "Stitcher already closed" }

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
        if (err != 0) throw OpenPanoException.fromCode(err)
    }

    /**
     * Add an image from raw RGB pixel data.
     *
     * @param pixels RGB pixel data (3 bytes per pixel)
     * @param width Image width
     * @param height Image height
     * @throws OpenPanoException if the operation fails
     */
    fun addImageData(pixels: ByteArray, width: Int, height: Int) {
        check(handle != 0L) { "Stitcher already closed" }
        val err = OpenPanoJNI.nativeAddImageData(handle, pixels, width, height)
        if (err != 0) throw OpenPanoException.fromCode(err)
    }

    /**
     * Number of images currently added.
     */
    val imageCount: Int
        get() {
            check(handle != 0L) { "Stitcher already closed" }
            return OpenPanoJNI.nativeImageCount(handle)
        }

    /**
     * Clear all added images.
     */
    fun clear() {
        check(handle != 0L) { "Stitcher already closed" }
        val err = OpenPanoJNI.nativeClearImages(handle)
        if (err != 0) throw OpenPanoException.fromCode(err)
    }

    /**
     * Execute panorama stitching.
     *
     * This is a blocking operation that may take significant time.
     *
     * @return The stitched panorama image
     * @throws OpenPanoException if stitching fails
     */
    fun stitch(): PanoramaImage {
        check(handle != 0L) { "Stitcher already closed" }
        val result = OpenPanoJNI.nativeStitch(handle)
        if (result == 0L) throw OpenPanoException.StitchingFailed()
        return PanoramaImage(result)
    }

    /**
     * Execute panorama stitching with progress callback.
     *
     * @param onProgress Called with progress (0-1) and stage description
     * @return The stitched panorama image
     * @throws OpenPanoException if stitching fails
     */
    fun stitch(onProgress: (Float, String) -> Unit): PanoramaImage {
        check(handle != 0L) { "Stitcher already closed" }

        val callback = object : OpenPanoJNI.ProgressCallback {
            override fun onProgress(progress: Float, stage: String) {
                onProgress(progress, stage)
            }
        }

        val result = OpenPanoJNI.nativeStitchWithProgress(handle, callback)
        if (result == 0L) throw OpenPanoException.StitchingFailed()
        return PanoramaImage(result)
    }

    /**
     * Cancel an ongoing stitching operation.
     *
     * Can be called from another thread.
     */
    fun cancel() {
        if (handle != 0L) {
            OpenPanoJNI.nativeCancel(handle)
        }
    }

    /**
     * Release native resources.
     *
     * Safe to call multiple times.
     */
    override fun close() {
        if (handle != 0L) {
            OpenPanoJNI.nativeDestroyStitcher(handle)
            handle = 0L
        }
    }
}

// MARK: - Global Functions

/**
 * Get the OpenPano library version string.
 */
fun openPanoVersion(): String = OpenPanoJNI.nativeVersion()

/**
 * Check if JPEG format is supported.
 */
fun hasJpegSupport(): Boolean = OpenPanoJNI.nativeHasJpegSupport()

/**
 * Check if PNG format is supported.
 */
fun hasPngSupport(): Boolean = OpenPanoJNI.nativeHasPngSupport()

/**
 * Get the number of available CPU cores.
 */
fun cpuCount(): Int = OpenPanoJNI.nativeGetCpuCount()

/**
 * Set the global log level.
 */
fun setLogLevel(level: LogLevel) {
    OpenPanoJNI.nativeSetLogLevel(level.value)
}

