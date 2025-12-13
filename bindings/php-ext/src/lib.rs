//! OpenPano PHP Extension
#![cfg_attr(windows, feature(abi_vectorcall))]

use ext_php_rs::prelude::*;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;

extern "C" {
    #[link_name = "openpano_version"]
    fn c_openpano_version() -> *const c_char;
    
    #[link_name = "openpano_stitcher_create"]
    fn c_openpano_stitcher_create(config: *const OpenpanoConfig) -> *mut std::ffi::c_void;
    
    #[link_name = "openpano_add_image_file"]
    fn c_openpano_add_image_file(stitcher: *mut std::ffi::c_void, path: *const c_char) -> i32;
    
    #[link_name = "openpano_stitch"]
    fn c_openpano_stitch(stitcher: *mut std::ffi::c_void, result: *mut *mut std::ffi::c_void) -> i32;
    
    #[link_name = "openpano_image_save"]
    fn c_openpano_image_save(image: *mut std::ffi::c_void, path: *const c_char, quality: i32) -> i32;
    
    #[link_name = "openpano_image_destroy"]
    fn c_openpano_image_destroy(image: *mut std::ffi::c_void);
    
    #[link_name = "openpano_stitcher_destroy"]
    fn c_openpano_stitcher_destroy(stitcher: *mut std::ffi::c_void);
    
    #[link_name = "openpano_get_last_error"]
    fn c_openpano_get_last_error() -> *const c_char;
}

#[repr(C)]
struct OpenpanoConfig {
    mode: i32,              // OpenpanoMode enum
    focal_length: f32,
    ordered_input: i32,
    crop_result: i32,
    num_threads: i32,
    lazy_read: i32,
    straighten: i32,
    max_output_size: i32,
    multiband: i32,
}

/// Get OpenPano library version
///
/// @return string The library version
#[php_function]
pub fn hello_openpano(name: String) -> String {
    format!("Hello from OpenPano, {}!", name)
}

/// Stitch images into panorama (simple API)
///
/// @param array $paths Image file paths
/// @param string $output Output panorama path  
/// @return bool Success
#[php_function]
pub fn stitch_panorama(paths: Vec<String>, output: String) -> PhpResult<bool> {
    openpano_stitch_images(paths, output, Some(90), Some(true))
}

/// Stitch images into panorama with quality and crop options
///
/// @param array $image_paths Image file paths
/// @param string $output_path Output panorama path
/// @param int $quality JPEG quality (1-100, default 90)
/// @param bool $crop Whether to crop the result (default true)
/// @return bool Success
#[php_function]
pub fn openpano_stitch_images(
    image_paths: Vec<String>, 
    output_path: String,
    quality: Option<i64>,
    crop: Option<bool>
) -> PhpResult<bool> {
    if image_paths.len() < 2 {
        return Err("Need at least 2 images".into());
    }

    let quality = quality.unwrap_or(90).max(1).min(100) as i32;
    let crop = crop.unwrap_or(true);

    let c_config = OpenpanoConfig {
        mode: 1,                    // OPENPANO_MODE_ESTIMATE_CAMERA (0=CYLINDER, 1=ESTIMATE, 2=TRANSLATION)
        focal_length: 36.0,
        ordered_input: 0,           // auto-detect order
        crop_result: if crop { 1 } else { 0 },
        num_threads: 0,             // auto
        lazy_read: 0,
        straighten: 0,
        max_output_size: 8000,      // Maximum output dimension (from config.cfg)
        multiband: 1,               // enabled
    };

    let handle = unsafe { c_openpano_stitcher_create(&c_config) };
    if handle.is_null() {
        return Err("Failed to create stitcher".into());
    }

    for (idx, path) in image_paths.iter().enumerate() {
        let c_path = CString::new(path.as_str()).map_err(|_| "Invalid path")?;
        let result = unsafe { c_openpano_add_image_file(handle, c_path.as_ptr()) };
        if result != 0 {
            let error_msg = unsafe {
                let err_ptr = c_openpano_get_last_error();
                if !err_ptr.is_null() {
                    CStr::from_ptr(err_ptr).to_string_lossy().into_owned()
                } else {
                    format!("Failed to load image {} ({})", idx + 1, path)
                }
            };
            unsafe { c_openpano_stitcher_destroy(handle); }
            return Err(error_msg.into());
        }
    }

    let mut result_handle: *mut std::ffi::c_void = std::ptr::null_mut();
    let stitch_result = unsafe { c_openpano_stitch(handle, &mut result_handle) };
    
    if stitch_result != 0 || result_handle.is_null() {
        let error_msg = unsafe {
            let err_ptr = c_openpano_get_last_error();
            if !err_ptr.is_null() {
                CStr::from_ptr(err_ptr).to_string_lossy().into_owned()
            } else {
                "Stitching failed - check that images have overlapping regions and good features".to_string()
            }
        };
        unsafe { c_openpano_stitcher_destroy(handle); }
        return Err(error_msg.into());
    }

    let c_output = CString::new(output_path).map_err(|_| "Invalid output path")?;
    let save_result = unsafe {
        c_openpano_image_save(result_handle, c_output.as_ptr(), quality)
    };
    
    unsafe {
        c_openpano_image_destroy(result_handle);
        c_openpano_stitcher_destroy(handle);
    }
    
    if save_result != 0 {
        return Err("Failed to save".into());
    }
    
    Ok(true)
}

#[php_module]
pub fn get_module(module: ModuleBuilder) -> ModuleBuilder {
    module
        .function(wrap_function!(hello_openpano))
        .function(wrap_function!(stitch_panorama))
        .function(wrap_function!(openpano_stitch_images))
}
