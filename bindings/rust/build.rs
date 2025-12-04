//! Build script for OpenPano Rust bindings
//!
//! This script handles:
//! 1. Finding or building the OpenPano library
//! 2. Generating Rust bindings from the C header

use std::env;
use std::path::PathBuf;

fn main() {
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    
    // Determine library search path
    // First, check if we have a pre-built library in the dist folder
    let project_root = manifest_dir
        .parent()
        .and_then(|p| p.parent())
        .expect("Could not find project root");
    
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    
    // Determine the platform-specific library path
    let lib_path = match target_os.as_str() {
        "macos" => project_root.join("dist/macos/lib"),
        "linux" => project_root.join("dist/linux/lib"),
        "windows" => project_root.join("dist/windows/lib"),
        "ios" => {
            // For iOS, we need to point to the xcframework
            project_root.join("dist/ios/OpenPano.xcframework")
        }
        "android" => {
            // For Android, select the appropriate ABI
            let abi = match target_arch.as_str() {
                "arm" => "armeabi-v7a",
                "aarch64" => "arm64-v8a",
                "x86" => "x86",
                "x86_64" => "x86_64",
                _ => "arm64-v8a",
            };
            project_root.join(format!("dist/android/{}", abi))
        }
        _ => project_root.join("dist/linux/lib"),
    };
    
    // Try to find the build directory first (has all libraries)
    let build_path = match target_os.as_str() {
        "macos" => {
            let arm64_path = project_root.join("build/macos-arm64");
            let x64_path = project_root.join("build/macos-x64");
            if arm64_path.join("libopenpano_all.a").exists() {
                arm64_path
            } else if x64_path.join("libopenpano_all.a").exists() {
                x64_path
            } else {
                lib_path.clone()
            }
        }
        _ => lib_path.clone(),
    };
    
    // Check if the combined library exists in build directory
    let combined_lib_exists = build_path.join("libopenpano_all.a").exists();
    
    if combined_lib_exists {
        println!("cargo:rustc-link-search=native={}", build_path.display());
        // Link the combined library that includes everything (OpenPano + FFI + lodepng)
        println!("cargo:rustc-link-lib=static=openpano_all");
        // Also link lodepng separately in case it's not fully included
        if build_path.join("liblodepng.a").exists() {
            println!("cargo:rustc-link-lib=static=lodepng");
        }
    } else if lib_path.join("libopenpano.a").exists() {
        println!("cargo:rustc-link-search=native={}", lib_path.display());
        println!("cargo:rustc-link-lib=static=openpano");
    } else {
        println!("cargo:warning=OpenPano library not found. Run build-all.sh first.");
        println!("cargo:rustc-link-lib=static=openpano");
    }
    
    // Link system dependencies based on platform
    match target_os.as_str() {
        "macos" => {
            println!("cargo:rustc-link-lib=c++");
            println!("cargo:rustc-link-lib=framework=Accelerate");
        }
        "linux" => {
            println!("cargo:rustc-link-lib=stdc++");
            println!("cargo:rustc-link-lib=pthread");
            println!("cargo:rustc-link-lib=m");
        }
        "windows" => {
            println!("cargo:rustc-link-lib=stdc++");
        }
        "ios" => {
            println!("cargo:rustc-link-lib=c++");
        }
        "android" => {
            println!("cargo:rustc-link-lib=c++_shared");
            println!("cargo:rustc-link-lib=log");
        }
        _ => {
            println!("cargo:rustc-link-lib=stdc++");
        }
    }
    
    // Link JPEG library (required for JPEG image loading)
    // Try pkg-config first, fall back to system library
    if pkg_config::probe_library("libjpeg").is_err() {
        println!("cargo:rustc-link-lib=jpeg");
    }
    
    // Generate bindings using bindgen
    let header_path = project_root.join("ffi/include/openpano_ffi.h");
    
    if !header_path.exists() {
        panic!(
            "Header file not found at {}. Make sure you're building from the correct directory.",
            header_path.display()
        );
    }
    
    let bindings = bindgen::Builder::default()
        .header(header_path.to_str().unwrap())
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // Only generate bindings for our API
        .allowlist_function("openpano_.*")
        .allowlist_type("Openpano.*")
        .allowlist_var("OPENPANO_.*")
        // Generate Rust enums for C enums
        .rustified_enum("OpenpanoError")
        .rustified_enum("OpenpanoMode")
        .rustified_enum("OpenpanoLogLevel")
        // Derive common traits
        .derive_debug(true)
        .derive_default(true)
        .derive_copy(true)
        .generate()
        .expect("Unable to generate bindings");
    
    // Write bindings to OUT_DIR
    bindings
        .write_to_file(out_dir.join("bindings.rs"))
        .expect("Couldn't write bindings!");
    
    // Rerun if header changes
    println!("cargo:rerun-if-changed={}", header_path.display());
    println!("cargo:rerun-if-changed=build.rs");
}

