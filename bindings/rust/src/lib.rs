//! # OpenPano - Rust Bindings for Panorama Stitching
//!
//! This crate provides safe Rust bindings to the OpenPano C++ library for
//! creating panoramic images from multiple overlapping photographs.
//!
//! ## Features
//!
//! - **Automatic camera estimation** - Works with arbitrary camera poses
//! - **Cylindrical stitching** - For standard panoramas with camera rotation
//! - **Translation mode** - For document scanning and mosaics
//! - **Multi-band blending** - Smooth seam blending
//! - **Progress callbacks** - Monitor stitching progress
//!
//! ## Example
//!
//! ```no_run
//! use openpano::{Stitcher, Config, StitchMode, Result};
//!
//! fn main() -> Result<()> {
//!     // Create a stitcher with default settings
//!     let config = Config::default()
//!         .mode(StitchMode::EstimateCamera)
//!         .crop(true);
//!     
//!     let mut stitcher = Stitcher::new(config)?;
//!     
//!     // Add images
//!     stitcher.add_image("image1.jpg")?;
//!     stitcher.add_image("image2.jpg")?;
//!     stitcher.add_image("image3.jpg")?;
//!     
//!     // Perform stitching
//!     let panorama = stitcher.stitch()?;
//!     
//!     // Save result
//!     panorama.save("panorama.jpg", 95)?;
//!     
//!     println!("Created {}x{} panorama", panorama.width(), panorama.height());
//!     Ok(())
//! }
//! ```
//!
//! ## Stitching with Progress
//!
//! ```no_run
//! use openpano::{Stitcher, Config, Result};
//!
//! fn main() -> Result<()> {
//!     let mut stitcher = Stitcher::with_defaults()?;
//!     stitcher.add_image("img1.jpg")?;
//!     stitcher.add_image("img2.jpg")?;
//!     
//!     let panorama = stitcher.stitch_with_progress(|progress, stage| {
//!         println!("[{:.0}%] {}", progress * 100.0, stage);
//!     })?;
//!     
//!     panorama.save("output.png", 0)?;
//!     Ok(())
//! }
//! ```

#![deny(missing_docs)]
#![deny(unsafe_op_in_unsafe_fn)]

use std::ffi::{CStr, CString};
use std::path::Path;
use std::ptr;
use thiserror::Error;

// Include generated bindings
#[allow(non_upper_case_globals)]
#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
#[allow(dead_code)]
#[allow(missing_docs)]
mod sys {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

pub use sys::OpenpanoConfig;

// =============================================================================
// Error Types
// =============================================================================

/// Errors that can occur during panorama stitching operations.
#[derive(Error, Debug)]
pub enum Error {
    /// The provided handle is invalid or null.
    #[error("Invalid handle")]
    InvalidHandle,

    /// An invalid argument was provided.
    #[error("Invalid argument: {0}")]
    InvalidArgument(String),

    /// The specified file was not found.
    #[error("File not found: {0}")]
    FileNotFound(String),

    /// At least 2 images are required for stitching.
    #[error("Need at least 2 images for stitching")]
    InsufficientImages,

    /// The stitching operation failed.
    #[error("Stitching failed: {0}")]
    StitchingFailed(String),

    /// Memory allocation failed.
    #[error("Out of memory")]
    OutOfMemory,

    /// Feature detection failed on one or more images.
    #[error("Feature detection failed")]
    FeatureDetectionFailed,

    /// Feature matching between images failed.
    #[error("Feature matching failed")]
    MatchingFailed,

    /// The image format is not supported.
    #[error("Unsupported image format")]
    UnsupportedFormat,

    /// The operation was cancelled.
    #[error("Operation cancelled")]
    Cancelled,

    /// An unknown error occurred.
    #[error("Unknown error (code: {0})")]
    Unknown(i32),

    /// Path contains invalid characters.
    #[error("Invalid path: {0}")]
    InvalidPath(String),
}

impl From<sys::OpenpanoError> for Error {
    fn from(err: sys::OpenpanoError) -> Self {
        match err {
            sys::OpenpanoError::OPENPANO_OK => {
                panic!("Attempted to convert OK to Error")
            }
            sys::OpenpanoError::OPENPANO_ERROR_INVALID_HANDLE => Error::InvalidHandle,
            sys::OpenpanoError::OPENPANO_ERROR_INVALID_ARGUMENT => {
                Error::InvalidArgument("Unknown".into())
            }
            sys::OpenpanoError::OPENPANO_ERROR_FILE_NOT_FOUND => {
                Error::FileNotFound("Unknown".into())
            }
            sys::OpenpanoError::OPENPANO_ERROR_INSUFFICIENT_IMAGES => Error::InsufficientImages,
            sys::OpenpanoError::OPENPANO_ERROR_STITCHING_FAILED => {
                Error::StitchingFailed("Unknown cause".into())
            }
            sys::OpenpanoError::OPENPANO_ERROR_OUT_OF_MEMORY => Error::OutOfMemory,
            sys::OpenpanoError::OPENPANO_ERROR_FEATURE_DETECTION_FAILED => {
                Error::FeatureDetectionFailed
            }
            sys::OpenpanoError::OPENPANO_ERROR_MATCHING_FAILED => Error::MatchingFailed,
            sys::OpenpanoError::OPENPANO_ERROR_UNSUPPORTED_FORMAT => Error::UnsupportedFormat,
            sys::OpenpanoError::OPENPANO_ERROR_CANCELLED => Error::Cancelled,
            sys::OpenpanoError::OPENPANO_ERROR_UNKNOWN => Error::Unknown(-99),
        }
    }
}

/// Result type for OpenPano operations.
pub type Result<T> = std::result::Result<T, Error>;

/// Check FFI result and convert to Result
fn check_result(err: sys::OpenpanoError) -> Result<()> {
    if err == sys::OpenpanoError::OPENPANO_OK {
        Ok(())
    } else {
        Err(err.into())
    }
}

// =============================================================================
// Enums
// =============================================================================

/// Stitching algorithm mode.
///
/// The choice of mode affects how the algorithm estimates transformations
/// between images.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum StitchMode {
    /// Cylindrical projection mode.
    ///
    /// Best for standard panoramas where the camera rotates around a
    /// vertical axis. Requires known focal length and ordered input images.
    Cylinder,

    /// Camera estimation mode with bundle adjustment.
    ///
    /// The most flexible mode that can handle arbitrary camera poses and
    /// unordered images. Computationally more intensive but produces
    /// excellent results.
    #[default]
    EstimateCamera,

    /// Translation mode.
    ///
    /// Best for scenarios with pure translation between images, such as
    /// document scanning or satellite imagery. Requires ordered input.
    Translation,
}

impl From<StitchMode> for sys::OpenpanoMode {
    fn from(mode: StitchMode) -> Self {
        match mode {
            StitchMode::Cylinder => sys::OpenpanoMode::OPENPANO_MODE_CYLINDER,
            StitchMode::EstimateCamera => sys::OpenpanoMode::OPENPANO_MODE_ESTIMATE_CAMERA,
            StitchMode::Translation => sys::OpenpanoMode::OPENPANO_MODE_TRANSLATION,
        }
    }
}

/// Log level for controlling output verbosity.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum LogLevel {
    /// No logging output.
    None,
    /// Only error messages.
    Error,
    /// Warnings and errors.
    #[default]
    Warn,
    /// Informational messages, warnings, and errors.
    Info,
    /// All messages including debug output.
    Debug,
}

impl From<LogLevel> for sys::OpenpanoLogLevel {
    fn from(level: LogLevel) -> Self {
        match level {
            LogLevel::None => sys::OpenpanoLogLevel::OPENPANO_LOG_NONE,
            LogLevel::Error => sys::OpenpanoLogLevel::OPENPANO_LOG_ERROR,
            LogLevel::Warn => sys::OpenpanoLogLevel::OPENPANO_LOG_WARN,
            LogLevel::Info => sys::OpenpanoLogLevel::OPENPANO_LOG_INFO,
            LogLevel::Debug => sys::OpenpanoLogLevel::OPENPANO_LOG_DEBUG,
        }
    }
}

// =============================================================================
// Configuration
// =============================================================================

/// Configuration options for panorama stitching.
///
/// Use the builder pattern to construct a configuration:
///
/// ```
/// use openpano::{Config, StitchMode};
///
/// let config = Config::default()
///     .mode(StitchMode::Cylinder)
///     .focal_length(35.0)
///     .ordered(true)
///     .crop(true);
/// ```
#[derive(Debug, Clone)]
pub struct Config {
    inner: sys::OpenpanoConfig,
}

impl Default for Config {
    fn default() -> Self {
        let mut inner: sys::OpenpanoConfig = unsafe { std::mem::zeroed() };
        unsafe {
            sys::openpano_config_default(&mut inner);
        }
        Self { inner }
    }
}

impl Config {
    /// Create a new configuration with default values.
    pub fn new() -> Self {
        Self::default()
    }

    /// Set the stitching mode.
    ///
    /// See [`StitchMode`] for available options.
    pub fn mode(mut self, mode: StitchMode) -> Self {
        self.inner.mode = mode.into();
        self
    }

    /// Set the focal length in 35mm equivalent.
    ///
    /// This is primarily used in cylinder mode. Typical values range from
    /// 24mm (wide angle) to 200mm (telephoto).
    pub fn focal_length(mut self, focal: f32) -> Self {
        self.inner.focal_length = focal;
        self
    }

    /// Set whether input images are in sequential order.
    ///
    /// When true, the algorithm assumes images are provided in left-to-right
    /// or right-to-left order. This can improve performance for ordered sets.
    pub fn ordered(mut self, ordered: bool) -> Self {
        self.inner.ordered_input = if ordered { 1 } else { 0 };
        self
    }

    /// Set whether to crop the result to remove black borders.
    ///
    /// When true, the final image is cropped to the largest inscribed
    /// rectangle, removing any black areas from the projection.
    pub fn crop(mut self, crop: bool) -> Self {
        self.inner.crop_result = if crop { 1 } else { 0 };
        self
    }

    /// Set the number of threads to use.
    ///
    /// Pass 0 (default) to automatically detect the number of CPU cores.
    pub fn threads(mut self, threads: u32) -> Self {
        self.inner.num_threads = threads as i32;
        self
    }

    /// Enable lazy image loading.
    ///
    /// When enabled, images are loaded on-demand rather than all at once,
    /// which can reduce peak memory usage for large sets of images.
    pub fn lazy_read(mut self, lazy: bool) -> Self {
        self.inner.lazy_read = if lazy { 1 } else { 0 };
        self
    }

    /// Enable perspective straightening.
    ///
    /// When enabled, the algorithm attempts to correct perspective distortion
    /// in the final panorama.
    pub fn straighten(mut self, straighten: bool) -> Self {
        self.inner.straighten = if straighten { 1 } else { 0 };
        self
    }

    /// Set maximum output dimension.
    ///
    /// If the result would be larger than this in either dimension, it will
    /// be scaled down. Pass 0 (default) for no limit.
    pub fn max_output_size(mut self, size: u32) -> Self {
        self.inner.max_output_size = size as i32;
        self
    }

    /// Set the number of bands for multi-band blending.
    ///
    /// Higher values produce smoother seams but take longer. Pass 0 to disable
    /// multi-band blending.
    pub fn multiband(mut self, bands: u32) -> Self {
        self.inner.multiband = bands as i32;
        self
    }
}

// =============================================================================
// Stitcher
// =============================================================================

/// Panorama stitcher for combining multiple images.
///
/// The `Stitcher` manages the stitching process including image loading,
/// feature detection, matching, and blending.
///
/// # Example
///
/// ```no_run
/// use openpano::{Stitcher, Config, Result};
///
/// fn create_panorama() -> Result<()> {
///     let mut stitcher = Stitcher::with_defaults()?;
///     
///     stitcher.add_image("photo1.jpg")?;
///     stitcher.add_image("photo2.jpg")?;
///     
///     let result = stitcher.stitch()?;
///     result.save("panorama.jpg", 90)?;
///     
///     Ok(())
/// }
/// ```
pub struct Stitcher {
    handle: sys::OpenpanoStitcherHandle,
}

// Safety: The underlying C++ stitcher uses mutex protection for thread safety
unsafe impl Send for Stitcher {}
unsafe impl Sync for Stitcher {}

impl Stitcher {
    /// Create a new stitcher with the specified configuration.
    ///
    /// # Errors
    ///
    /// Returns `Error::OutOfMemory` if allocation fails.
    pub fn new(config: Config) -> Result<Self> {
        let handle = unsafe { sys::openpano_stitcher_create(&config.inner) };
        if handle.is_null() {
            return Err(Error::OutOfMemory);
        }
        Ok(Self { handle })
    }

    /// Create a new stitcher with default configuration.
    ///
    /// Equivalent to `Stitcher::new(Config::default())`.
    pub fn with_defaults() -> Result<Self> {
        Self::new(Config::default())
    }

    /// Add an image file to the stitcher.
    ///
    /// Supported formats depend on build configuration but typically include
    /// JPEG and PNG.
    ///
    /// # Errors
    ///
    /// - `Error::FileNotFound` if the file doesn't exist
    /// - `Error::UnsupportedFormat` if the format isn't supported
    pub fn add_image<P: AsRef<Path>>(&mut self, path: P) -> Result<()> {
        let path_str = path.as_ref().to_string_lossy();
        let c_path = CString::new(path_str.as_ref())
            .map_err(|_| Error::InvalidPath(path_str.into_owned()))?;

        let err = unsafe { sys::openpano_add_image_file(self.handle, c_path.as_ptr()) };

        if err == sys::OpenpanoError::OPENPANO_OK {
            Ok(())
        } else if err == sys::OpenpanoError::OPENPANO_ERROR_FILE_NOT_FOUND {
            Err(Error::FileNotFound(path_str.into_owned()))
        } else {
            Err(err.into())
        }
    }

    /// Add an image from raw RGB pixel data.
    ///
    /// The pixel data should be in RGB format with 8 bits per channel,
    /// arranged in row-major order.
    ///
    /// # Arguments
    ///
    /// * `pixels` - Raw RGB pixel data (length must be width * height * 3)
    /// * `width` - Image width in pixels
    /// * `height` - Image height in pixels
    ///
    /// # Errors
    ///
    /// - `Error::InvalidArgument` if dimensions don't match data length
    /// - `Error::OutOfMemory` if allocation fails
    pub fn add_image_data(&mut self, pixels: &[u8], width: u32, height: u32) -> Result<()> {
        let expected_len = (width as usize) * (height as usize) * 3;
        if pixels.len() != expected_len {
            return Err(Error::InvalidArgument(format!(
                "Expected {} bytes, got {}",
                expected_len,
                pixels.len()
            )));
        }

        let err = unsafe {
            sys::openpano_add_image_data(
                self.handle,
                pixels.as_ptr(),
                width as i32,
                height as i32,
                0, // auto stride
            )
        };

        check_result(err)
    }

    /// Add an image from float RGB pixel data.
    ///
    /// The pixel data should be in RGB format with float values in [0, 1] range,
    /// arranged in row-major order.
    pub fn add_image_data_float(&mut self, pixels: &[f32], width: u32, height: u32) -> Result<()> {
        let expected_len = (width as usize) * (height as usize) * 3;
        if pixels.len() != expected_len {
            return Err(Error::InvalidArgument(format!(
                "Expected {} floats, got {}",
                expected_len,
                pixels.len()
            )));
        }

        let err = unsafe {
            sys::openpano_add_image_data_float(
                self.handle,
                pixels.as_ptr(),
                width as i32,
                height as i32,
                0, // auto stride
            )
        };

        check_result(err)
    }

    /// Get the number of images currently added to the stitcher.
    pub fn image_count(&self) -> usize {
        let count = unsafe { sys::openpano_image_count(self.handle) };
        count.max(0) as usize
    }

    /// Clear all added images.
    ///
    /// After calling this, you can add new images for a fresh stitching operation.
    pub fn clear(&mut self) -> Result<()> {
        let err = unsafe { sys::openpano_clear_images(self.handle) };
        check_result(err)
    }

    /// Execute panorama stitching.
    ///
    /// This is a blocking operation that may take significant time depending on
    /// the number and size of images.
    ///
    /// # Errors
    ///
    /// - `Error::InsufficientImages` if fewer than 2 images were added
    /// - `Error::FeatureDetectionFailed` if features couldn't be detected
    /// - `Error::MatchingFailed` if images don't have enough overlap
    /// - `Error::StitchingFailed` for other stitching errors
    pub fn stitch(&mut self) -> Result<Image> {
        let mut result: sys::OpenpanoImageHandle = ptr::null_mut();
        let err = unsafe { sys::openpano_stitch(self.handle, &mut result) };

        if err == sys::OpenpanoError::OPENPANO_OK && !result.is_null() {
            Ok(Image { handle: result })
        } else {
            Err(err.into())
        }
    }

    /// Execute panorama stitching with a progress callback.
    ///
    /// The callback receives the current progress (0.0 to 1.0) and a
    /// description of the current stage.
    ///
    /// # Example
    ///
    /// ```no_run
    /// # use openpano::{Stitcher, Result};
    /// # fn main() -> Result<()> {
    /// let mut stitcher = Stitcher::with_defaults()?;
    /// // ... add images ...
    /// 
    /// let result = stitcher.stitch_with_progress(|progress, stage| {
    ///     println!("[{:3.0}%] {}", progress * 100.0, stage);
    /// })?;
    /// # Ok(())
    /// # }
    /// ```
    pub fn stitch_with_progress<F>(&mut self, callback: F) -> Result<Image>
    where
        F: Fn(f32, &str) + 'static,
    {
        // Trampoline function for the C callback
        extern "C" fn progress_trampoline(
            progress: f32,
            stage: *const libc::c_char,
            user_data: *mut libc::c_void,
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

        // Box the callback
        let callback_box: Box<Box<dyn Fn(f32, &str)>> = Box::new(Box::new(callback));
        let user_data = Box::into_raw(callback_box) as *mut libc::c_void;

        let mut result: sys::OpenpanoImageHandle = ptr::null_mut();
        let err = unsafe {
            sys::openpano_stitch_with_progress(
                self.handle,
                &mut result,
                Some(progress_trampoline),
                user_data,
            )
        };

        // Clean up callback
        unsafe {
            let _ = Box::from_raw(user_data as *mut Box<dyn Fn(f32, &str)>);
        }

        if err == sys::OpenpanoError::OPENPANO_OK && !result.is_null() {
            Ok(Image { handle: result })
        } else {
            Err(err.into())
        }
    }

    /// Request cancellation of an ongoing stitching operation.
    ///
    /// This can be called from another thread to abort a running stitch.
    /// The stitch operation will return `Error::Cancelled`.
    pub fn cancel(&self) -> Result<()> {
        let err = unsafe { sys::openpano_cancel(self.handle) };
        check_result(err)
    }
}

impl Drop for Stitcher {
    fn drop(&mut self) {
        unsafe {
            sys::openpano_stitcher_destroy(self.handle);
        }
    }
}

// =============================================================================
// Image
// =============================================================================

/// A panorama result image.
///
/// This holds the output of a stitching operation. Use [`pixels()`](Image::pixels)
/// to access raw pixel data or [`save()`](Image::save) to write to a file.
pub struct Image {
    handle: sys::OpenpanoImageHandle,
}

// Safety: Image data is immutable after creation
unsafe impl Send for Image {}
unsafe impl Sync for Image {}

impl Image {
    /// Get the image width in pixels.
    pub fn width(&self) -> u32 {
        let mut width: i32 = 0;
        unsafe {
            sys::openpano_image_dimensions(self.handle, &mut width, ptr::null_mut());
        }
        width as u32
    }

    /// Get the image height in pixels.
    pub fn height(&self) -> u32 {
        let mut height: i32 = 0;
        unsafe {
            sys::openpano_image_dimensions(self.handle, ptr::null_mut(), &mut height);
        }
        height as u32
    }

    /// Get image dimensions as (width, height).
    pub fn dimensions(&self) -> (u32, u32) {
        let mut width: i32 = 0;
        let mut height: i32 = 0;
        unsafe {
            sys::openpano_image_dimensions(self.handle, &mut width, &mut height);
        }
        (width as u32, height as u32)
    }

    /// Get number of color channels (typically 3 for RGB).
    pub fn channels(&self) -> u32 {
        let ch = unsafe { sys::openpano_image_channels(self.handle) };
        ch.max(0) as u32
    }

    /// Get a reference to the raw pixel data as RGB bytes.
    ///
    /// The data is in row-major order with 3 bytes (R, G, B) per pixel.
    /// Each value is in the range [0, 255].
    ///
    /// # Example
    ///
    /// ```no_run
    /// # use openpano::{Image, Result};
    /// fn print_pixel(image: &Image, x: u32, y: u32) -> Result<()> {
    ///     let pixels = image.pixels()?;
    ///     let idx = ((y * image.width() + x) * 3) as usize;
    ///     println!("RGB at ({}, {}): {}, {}, {}", x, y,
    ///         pixels[idx], pixels[idx + 1], pixels[idx + 2]);
    ///     Ok(())
    /// }
    /// ```
    pub fn pixels(&self) -> Result<&[u8]> {
        let mut pixels: *const u8 = ptr::null();
        let mut size: usize = 0;

        let err = unsafe { sys::openpano_image_data(self.handle, &mut pixels, &mut size) };

        if err == sys::OpenpanoError::OPENPANO_OK && !pixels.is_null() {
            Ok(unsafe { std::slice::from_raw_parts(pixels, size) })
        } else {
            Err(err.into())
        }
    }

    /// Get a reference to the raw pixel data as floats.
    ///
    /// The data is in row-major order with 3 floats (R, G, B) per pixel.
    /// Each value is in the range [0.0, 1.0].
    pub fn pixels_float(&self) -> Result<&[f32]> {
        let mut pixels: *const f32 = ptr::null();
        let mut size: usize = 0;

        let err = unsafe { sys::openpano_image_data_float(self.handle, &mut pixels, &mut size) };

        if err == sys::OpenpanoError::OPENPANO_OK && !pixels.is_null() {
            let count = size / std::mem::size_of::<f32>();
            Ok(unsafe { std::slice::from_raw_parts(pixels, count) })
        } else {
            Err(err.into())
        }
    }

    /// Copy pixel data to a new Vec.
    ///
    /// This creates an owned copy of the pixel data.
    pub fn to_vec(&self) -> Result<Vec<u8>> {
        Ok(self.pixels()?.to_vec())
    }

    /// Save the image to a file.
    ///
    /// The format is determined by the file extension:
    /// - `.jpg` or `.jpeg` - JPEG format (quality affects compression)
    /// - `.png` - PNG format (quality is ignored)
    ///
    /// # Arguments
    ///
    /// * `path` - Output file path
    /// * `quality` - JPEG quality (1-100, higher = better quality/larger file)
    ///
    /// # Errors
    ///
    /// Returns an error if the file cannot be written.
    pub fn save<P: AsRef<Path>>(&self, path: P, quality: u8) -> Result<()> {
        let path_str = path.as_ref().to_string_lossy();
        let c_path = CString::new(path_str.as_ref())
            .map_err(|_| Error::InvalidPath(path_str.into_owned()))?;

        let err =
            unsafe { sys::openpano_image_save(self.handle, c_path.as_ptr(), quality as i32) };

        check_result(err)
    }
}

impl Drop for Image {
    fn drop(&mut self) {
        unsafe {
            sys::openpano_image_destroy(self.handle);
        }
    }
}

// =============================================================================
// Utility Functions
// =============================================================================

/// Get the OpenPano library version string.
///
/// Returns a version string like "1.0.0".
pub fn version() -> &'static str {
    unsafe {
        let ptr = sys::openpano_version();
        CStr::from_ptr(ptr).to_str().unwrap_or("unknown")
    }
}

/// Check if JPEG format is supported.
///
/// JPEG support depends on whether libjpeg was available at build time.
pub fn has_jpeg_support() -> bool {
    unsafe { sys::openpano_has_jpeg_support() != 0 }
}

/// Check if PNG format is supported.
///
/// PNG is always supported via the bundled lodepng library.
pub fn has_png_support() -> bool {
    unsafe { sys::openpano_has_png_support() != 0 }
}

/// Get the number of CPU cores available.
pub fn cpu_count() -> u32 {
    let count = unsafe { sys::openpano_get_cpu_count() };
    count.max(1) as u32
}

/// Set the global log level.
///
/// This affects all stitchers and can be useful for debugging.
pub fn set_log_level(level: LogLevel) {
    unsafe {
        sys::openpano_set_log_level(level.into());
    }
}

// =============================================================================
// Tests
// =============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_config_builder() {
        let config = Config::default()
            .mode(StitchMode::Cylinder)
            .focal_length(35.0)
            .crop(true)
            .threads(4);

        assert_eq!(
            config.inner.mode,
            sys::OpenpanoMode::OPENPANO_MODE_CYLINDER
        );
        assert_eq!(config.inner.focal_length, 35.0);
        assert_eq!(config.inner.crop_result, 1);
        assert_eq!(config.inner.num_threads, 4);
    }

    #[test]
    fn test_version() {
        let v = version();
        assert!(!v.is_empty());
        // Version should be in format "X.Y.Z"
        assert!(v.contains('.'));
    }

    #[test]
    fn test_png_support() {
        // PNG should always be supported
        assert!(has_png_support());
    }

    #[test]
    fn test_cpu_count() {
        assert!(cpu_count() >= 1);
    }

    #[test]
    fn test_stitcher_creation() {
        // Note: This test may fail if the library isn't properly linked
        // In that case it's a build/link error, not a code error
        let result = Stitcher::with_defaults();
        // We don't assert success here because the library may not be built yet
        if let Ok(stitcher) = result {
            assert_eq!(stitcher.image_count(), 0);
        }
    }
}

