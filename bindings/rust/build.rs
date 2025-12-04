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
    
    // Check if the library exists
    let lib_exists = if target_os == "ios" {
        lib_path.exists()
    } else {
        lib_path.join("libopenpano.a").exists() || lib_path.join("openpano.lib").exists()
    };
    
    if lib_exists {
        println!("cargo:rustc-link-search=native={}", lib_path.display());
    } else {
        // Try to find in the build directory
        let build_path = project_root.join("build");
        if build_path.exists() {
            println!("cargo:rustc-link-search=native={}", build_path.display());
        } else {
            println!("cargo:warning=OpenPano library not found. Run build-all.sh first.");
        }
    }
    
    // Link the OpenPano library
    println!("cargo:rustc-link-lib=static=openpano");
    
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
    
    // Link JPEG if feature is enabled
    if cfg!(feature = "jpeg") {
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

