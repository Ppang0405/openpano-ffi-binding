// OpenPano.swift
// Swift bindings for OpenPano panorama stitching library
//
// Copyright (c) 2024 OpenPano Contributors
// MIT License

import Foundation
#if canImport(UIKit)
import UIKit
#elseif canImport(AppKit)
import AppKit
#endif
import CoreGraphics

// MARK: - Bridging Header Required
// Add to your bridging header:
// #import "openpano_ffi.h"

// MARK: - Stitch Mode

/// Stitching algorithm modes supported by OpenPano.
public enum StitchMode {
    /// Cylindrical projection mode.
    /// Best for standard panoramas where camera rotates around vertical axis.
    /// Requires ordered input and known focal length.
    case cylinder
    
    /// Camera estimation mode with bundle adjustment.
    /// Most flexible - works with arbitrary camera poses and unordered images.
    case estimateCamera
    
    /// Translation mode.
    /// Best for pure translation (document scanning, satellite imagery).
    /// Requires ordered input.
    case translation
    
    /// Convert to C enum value.
    var cValue: OpenpanoMode {
        switch self {
        case .cylinder: return OPENPANO_MODE_CYLINDER
        case .estimateCamera: return OPENPANO_MODE_ESTIMATE_CAMERA
        case .translation: return OPENPANO_MODE_TRANSLATION
        }
    }
}

// MARK: - Log Level

/// Log verbosity levels.
public enum LogLevel: Int32 {
    /// No logging output.
    case none = 0
    /// Only error messages.
    case error = 1
    /// Warnings and errors.
    case warn = 2
    /// Informational messages, warnings, and errors.
    case info = 3
    /// All messages including debug output.
    case debug = 4
    
    /// Convert to C enum value.
    var cValue: OpenpanoLogLevel {
        return OpenpanoLogLevel(rawValue: UInt32(self.rawValue))
    }
}

// MARK: - Errors

/// Errors that can occur during panorama stitching.
public enum OpenPanoError: Error, LocalizedError {
    /// The provided handle is invalid or null.
    case invalidHandle
    /// An invalid argument was provided.
    case invalidArgument(String)
    /// The specified file was not found.
    case fileNotFound(String)
    /// At least 2 images are required for stitching.
    case insufficientImages
    /// The stitching operation failed.
    case stitchingFailed(String)
    /// Memory allocation failed.
    case outOfMemory
    /// Feature detection failed on one or more images.
    case featureDetectionFailed
    /// Feature matching between images failed.
    case matchingFailed
    /// The image format is not supported.
    case unsupportedFormat
    /// The operation was cancelled.
    case cancelled
    /// An unknown error occurred.
    case unknown(Int32)
    
    /// Initialize from C error code.
    init(from error: OpenpanoError) {
        switch error {
        case OPENPANO_ERROR_INVALID_HANDLE: 
            self = .invalidHandle
        case OPENPANO_ERROR_INVALID_ARGUMENT: 
            self = .invalidArgument("")
        case OPENPANO_ERROR_FILE_NOT_FOUND: 
            self = .fileNotFound("")
        case OPENPANO_ERROR_INSUFFICIENT_IMAGES: 
            self = .insufficientImages
        case OPENPANO_ERROR_STITCHING_FAILED: 
            self = .stitchingFailed("")
        case OPENPANO_ERROR_OUT_OF_MEMORY: 
            self = .outOfMemory
        case OPENPANO_ERROR_FEATURE_DETECTION_FAILED: 
            self = .featureDetectionFailed
        case OPENPANO_ERROR_MATCHING_FAILED: 
            self = .matchingFailed
        case OPENPANO_ERROR_UNSUPPORTED_FORMAT:
            self = .unsupportedFormat
        case OPENPANO_ERROR_CANCELLED:
            self = .cancelled
        default: 
            self = .unknown(error.rawValue)
        }
    }
    
    /// Human-readable error description.
    public var errorDescription: String? {
        switch self {
        case .invalidHandle:
            return "Invalid handle"
        case .invalidArgument(let msg):
            return msg.isEmpty ? "Invalid argument" : "Invalid argument: \(msg)"
        case .fileNotFound(let path):
            return path.isEmpty ? "File not found" : "File not found: \(path)"
        case .insufficientImages:
            return "Need at least 2 images for stitching"
        case .stitchingFailed(let msg):
            return msg.isEmpty ? "Stitching failed" : "Stitching failed: \(msg)"
        case .outOfMemory:
            return "Out of memory"
        case .featureDetectionFailed:
            return "Feature detection failed"
        case .matchingFailed:
            return "Feature matching failed"
        case .unsupportedFormat:
            return "Unsupported image format"
        case .cancelled:
            return "Operation cancelled"
        case .unknown(let code):
            return "Unknown error (code: \(code))"
        }
    }
}

// MARK: - Configuration

/// Configuration options for panorama stitching.
///
/// Use the builder pattern to customize settings:
/// ```swift
/// let config = StitcherConfig()
///     .mode(.estimateCamera)
///     .crop(true)
///     .threads(4)
/// ```
public struct StitcherConfig {
    /// Stitching algorithm mode.
    public var mode: StitchMode = .estimateCamera
    
    /// Focal length in 35mm equivalent (primarily for cylinder mode).
    public var focalLength: Float = 36.0
    
    /// Whether input images are in sequential order.
    public var orderedInput: Bool = false
    
    /// Whether to crop result to remove black borders.
    public var cropResult: Bool = true
    
    /// Number of threads (0 = auto-detect).
    public var numThreads: Int = 0
    
    /// Enable lazy image loading to reduce memory.
    public var lazyRead: Bool = false
    
    /// Enable perspective straightening.
    public var straighten: Bool = false
    
    /// Maximum output dimension (0 = no limit).
    public var maxOutputSize: Int = 0
    
    /// Number of bands for multi-band blending (0 = disabled).
    public var multiband: Int = 1
    
    /// Create a new configuration with default values.
    public init() {}
    
    /// Set the stitching mode.
    public func mode(_ mode: StitchMode) -> StitcherConfig {
        var config = self
        config.mode = mode
        return config
    }
    
    /// Set the focal length in 35mm equivalent.
    public func focalLength(_ focal: Float) -> StitcherConfig {
        var config = self
        config.focalLength = focal
        return config
    }
    
    /// Set whether input images are ordered.
    public func ordered(_ ordered: Bool) -> StitcherConfig {
        var config = self
        config.orderedInput = ordered
        return config
    }
    
    /// Set whether to crop the result.
    public func crop(_ crop: Bool) -> StitcherConfig {
        var config = self
        config.cropResult = crop
        return config
    }
    
    /// Set the number of threads.
    public func threads(_ threads: Int) -> StitcherConfig {
        var config = self
        config.numThreads = threads
        return config
    }
    
    /// Enable or disable lazy reading.
    public func lazyRead(_ lazy: Bool) -> StitcherConfig {
        var config = self
        config.lazyRead = lazy
        return config
    }
    
    /// Convert to C config struct.
    func toCConfig() -> OpenpanoConfig {
        var config = OpenpanoConfig()
        openpano_config_default(&config)
        config.mode = mode.cValue
        config.focal_length = focalLength
        config.ordered_input = orderedInput ? 1 : 0
        config.crop_result = cropResult ? 1 : 0
        config.num_threads = Int32(numThreads)
        config.lazy_read = lazyRead ? 1 : 0
        config.straighten = straighten ? 1 : 0
        config.max_output_size = Int32(maxOutputSize)
        config.multiband = Int32(multiband)
        return config
    }
}

// MARK: - Panorama Image

/// A result image from stitching operations.
public class PanoramaImage {
    private var handle: OpenpanoImageHandle?
    
    /// Initialize with a C handle.
    init(handle: OpenpanoImageHandle) {
        self.handle = handle
    }
    
    deinit {
        close()
    }
    
    /// Release resources. Safe to call multiple times.
    public func close() {
        if let handle = handle {
            openpano_image_destroy(handle)
            self.handle = nil
        }
    }
    
    /// Image width in pixels.
    public var width: Int {
        guard let handle = handle else { return 0 }
        var width: Int32 = 0
        openpano_image_dimensions(handle, &width, nil)
        return Int(width)
    }
    
    /// Image height in pixels.
    public var height: Int {
        guard let handle = handle else { return 0 }
        var height: Int32 = 0
        openpano_image_dimensions(handle, nil, &height)
        return Int(height)
    }
    
    /// Image dimensions as (width, height).
    public var dimensions: (width: Int, height: Int) {
        guard let handle = handle else { return (0, 0) }
        var width: Int32 = 0
        var height: Int32 = 0
        openpano_image_dimensions(handle, &width, &height)
        return (Int(width), Int(height))
    }
    
    /// Number of color channels (typically 3 for RGB).
    public var channels: Int {
        guard let handle = handle else { return 0 }
        return Int(openpano_image_channels(handle))
    }
    
    /// Get raw pixel data as RGB bytes.
    /// - Returns: Pixel data in row-major order, 3 bytes per pixel.
    public func pixels() throws -> Data {
        guard let handle = handle else {
            throw OpenPanoError.invalidHandle
        }
        
        var pixels: UnsafePointer<UInt8>?
        var size: Int = 0
        
        let err = openpano_image_data(handle, &pixels, &size)
        guard err == OPENPANO_OK, let pixelData = pixels else {
            throw OpenPanoError(from: err)
        }
        
        return Data(bytes: pixelData, count: size)
    }
    
    #if canImport(UIKit)
    /// Convert to UIImage (iOS/tvOS).
    public func toUIImage() -> UIImage? {
        guard let handle = handle else { return nil }
        
        var pixels: UnsafePointer<UInt8>?
        var size: Int = 0
        
        let err = openpano_image_data(handle, &pixels, &size)
        guard err == OPENPANO_OK, let pixelData = pixels else { return nil }
        
        let dims = dimensions
        guard dims.width > 0 && dims.height > 0 else { return nil }
        
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
    #endif
    
    #if canImport(AppKit) && !targetEnvironment(macCatalyst)
    /// Convert to NSImage (macOS).
    public func toNSImage() -> NSImage? {
        guard let handle = handle else { return nil }
        
        var pixels: UnsafePointer<UInt8>?
        var size: Int = 0
        
        let err = openpano_image_data(handle, &pixels, &size)
        guard err == OPENPANO_OK, let pixelData = pixels else { return nil }
        
        let dims = dimensions
        guard dims.width > 0 && dims.height > 0 else { return nil }
        
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
        
        return NSImage(cgImage: cgImage, size: NSSize(width: dims.width, height: dims.height))
    }
    #endif
    
    /// Convert to CGImage.
    public func toCGImage() -> CGImage? {
        guard let handle = handle else { return nil }
        
        var pixels: UnsafePointer<UInt8>?
        var size: Int = 0
        
        let err = openpano_image_data(handle, &pixels, &size)
        guard err == OPENPANO_OK, let pixelData = pixels else { return nil }
        
        let dims = dimensions
        guard dims.width > 0 && dims.height > 0 else { return nil }
        
        let data = Data(bytes: pixelData, count: size)
        let colorSpace = CGColorSpaceCreateDeviceRGB()
        
        guard let provider = CGDataProvider(data: data as CFData) else { return nil }
        
        return CGImage(
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
        )
    }
    
    /// Save image to file.
    /// - Parameters:
    ///   - path: Output file path
    ///   - quality: JPEG quality (1-100, ignored for PNG)
    public func save(to path: String, quality: Int = 90) throws {
        guard let handle = handle else {
            throw OpenPanoError.invalidHandle
        }
        let err = openpano_image_save(handle, path, Int32(quality))
        guard err == OPENPANO_OK else {
            throw OpenPanoError(from: err)
        }
    }
    
    /// Save image to URL.
    /// - Parameters:
    ///   - url: Output file URL
    ///   - quality: JPEG quality (1-100, ignored for PNG)
    public func save(to url: URL, quality: Int = 90) throws {
        try save(to: url.path, quality: quality)
    }
}

// MARK: - Stitcher

/// Progress callback for stitching operations.
public typealias ProgressCallback = (Float, String) -> Void

/// Panorama stitcher for combining multiple images.
///
/// Example usage:
/// ```swift
/// let stitcher = try Stitcher()
/// try stitcher.addImage(path: "photo1.jpg")
/// try stitcher.addImage(path: "photo2.jpg")
/// let panorama = try stitcher.stitch()
/// try panorama.save(to: "panorama.jpg", quality: 90)
/// ```
public class Stitcher {
    private var handle: OpenpanoStitcherHandle?
    
    /// Create a new stitcher with the specified configuration.
    /// - Parameter config: Configuration options (uses defaults if nil)
    public init(config: StitcherConfig = StitcherConfig()) throws {
        var cConfig = config.toCConfig()
        handle = openpano_stitcher_create(&cConfig)
        guard handle != nil else {
            throw OpenPanoError.outOfMemory
        }
    }
    
    deinit {
        close()
    }
    
    /// Release resources. Safe to call multiple times.
    public func close() {
        if let handle = handle {
            openpano_stitcher_destroy(handle)
            self.handle = nil
        }
    }
    
    /// Add an image file to the stitcher.
    /// - Parameter path: Path to image file
    public func addImage(path: String) throws {
        guard let handle = handle else {
            throw OpenPanoError.invalidHandle
        }
        let err = openpano_add_image_file(handle, path)
        guard err == OPENPANO_OK else {
            if err == OPENPANO_ERROR_FILE_NOT_FOUND {
                throw OpenPanoError.fileNotFound(path)
            }
            throw OpenPanoError(from: err)
        }
    }
    
    /// Add an image from URL.
    /// - Parameter url: URL to image file
    public func addImage(url: URL) throws {
        try addImage(path: url.path)
    }
    
    #if canImport(UIKit)
    /// Add an image from UIImage.
    /// - Parameter image: UIImage to add
    public func addImage(_ image: UIImage) throws {
        guard let handle = handle,
              let cgImage = image.cgImage else {
            throw OpenPanoError.invalidArgument("Invalid UIImage")
        }
        
        try addCGImage(cgImage)
    }
    #endif
    
    #if canImport(AppKit) && !targetEnvironment(macCatalyst)
    /// Add an image from NSImage.
    /// - Parameter image: NSImage to add
    public func addImage(_ image: NSImage) throws {
        guard let handle = handle,
              let cgImage = image.cgImage(forProposedRect: nil, context: nil, hints: nil) else {
            throw OpenPanoError.invalidArgument("Invalid NSImage")
        }
        
        try addCGImage(cgImage)
    }
    #endif
    
    /// Add an image from CGImage.
    /// - Parameter cgImage: CGImage to add
    public func addCGImage(_ cgImage: CGImage) throws {
        guard let handle = handle else {
            throw OpenPanoError.invalidHandle
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
            throw OpenPanoError.invalidArgument("Failed to create context")
        }
        
        context.draw(cgImage, in: CGRect(x: 0, y: 0, width: width, height: height))
        
        let err = openpano_add_image_data(
            handle,
            &pixelData,
            Int32(width),
            Int32(height),
            0
        )
        guard err == OPENPANO_OK else {
            throw OpenPanoError(from: err)
        }
    }
    
    /// Add an image from raw RGB pixel data.
    /// - Parameters:
    ///   - pixels: RGB pixel data (3 bytes per pixel)
    ///   - width: Image width
    ///   - height: Image height
    public func addImageData(_ pixels: Data, width: Int, height: Int) throws {
        guard let handle = handle else {
            throw OpenPanoError.invalidHandle
        }
        
        let err = pixels.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) -> OpenpanoError in
            guard let baseAddress = ptr.baseAddress else {
                return OPENPANO_ERROR_INVALID_ARGUMENT
            }
            return openpano_add_image_data(
                handle,
                baseAddress.assumingMemoryBound(to: UInt8.self),
                Int32(width),
                Int32(height),
                0
            )
        }
        
        guard err == OPENPANO_OK else {
            throw OpenPanoError(from: err)
        }
    }
    
    /// Number of images currently added.
    public var imageCount: Int {
        guard let handle = handle else { return 0 }
        return Int(openpano_image_count(handle))
    }
    
    /// Clear all added images.
    public func clear() throws {
        guard let handle = handle else {
            throw OpenPanoError.invalidHandle
        }
        let err = openpano_clear_images(handle)
        guard err == OPENPANO_OK else {
            throw OpenPanoError(from: err)
        }
    }
    
    /// Execute panorama stitching.
    /// - Returns: The stitched panorama image
    public func stitch() throws -> PanoramaImage {
        guard let handle = handle else {
            throw OpenPanoError.invalidHandle
        }
        
        var result: OpenpanoImageHandle?
        let err = openpano_stitch(handle, &result)
        
        guard err == OPENPANO_OK, let imageHandle = result else {
            throw OpenPanoError(from: err)
        }
        
        return PanoramaImage(handle: imageHandle)
    }
    
    /// Cancel an ongoing stitching operation.
    /// Can be called from another thread/queue.
    public func cancel() {
        guard let handle = handle else { return }
        openpano_cancel(handle)
    }
}

// MARK: - Global Functions

/// Get the OpenPano library version string.
public func openPanoVersion() -> String {
    return String(cString: openpano_version())
}

/// Check if JPEG format is supported.
public func hasJPEGSupport() -> Bool {
    return openpano_has_jpeg_support() != 0
}

/// Check if PNG format is supported.
public func hasPNGSupport() -> Bool {
    return openpano_has_png_support() != 0
}

/// Get the number of available CPU cores.
public func cpuCount() -> Int {
    return Int(openpano_get_cpu_count())
}

/// Set the global log level.
public func setLogLevel(_ level: LogLevel) {
    openpano_set_log_level(level.cValue)
}

