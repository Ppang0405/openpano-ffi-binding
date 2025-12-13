use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    
    // Link to the OpenPano static library
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let lib_dir = manifest_dir
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .join("dist")
        .join("macos")
        .join("lib");
    
    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    println!("cargo:rustc-link-lib=static=openpano");
    
    // Link JPEG library (required by OpenPano)
    #[cfg(target_os = "macos")]
    {
        // On macOS with Homebrew
        if let Ok(jpeg_dir) = env::var("DEP_JPEG_ROOT") {
            println!("cargo:rustc-link-search=native={}/lib", jpeg_dir);
        } else {
            // Try common Homebrew paths
            if PathBuf::from("/opt/homebrew/lib").exists() {
                println!("cargo:rustc-link-search=native=/opt/homebrew/lib");
            } else if PathBuf::from("/usr/local/lib").exists() {
                println!("cargo:rustc-link-search=native=/usr/local/lib");
            }
        }
        println!("cargo:rustc-link-lib=dylib=jpeg");
        println!("cargo:rustc-link-lib=c++");
    }
    
    #[cfg(target_os = "linux")]
    {
        println!("cargo:rustc-link-lib=dylib=jpeg");
        println!("cargo:rustc-link-lib=stdc++");
    }
}

