# OpenPano FFI Portable

> Cross-platform FFI bindings for the OpenPano panorama stitching library.

This project provides portable C FFI bindings for [OpenPano](https://github.com/ppwwyyxx/OpenPano), enabling integration with Rust, Go, Swift, Kotlin, and other languages across desktop and mobile platforms.

## Features

- **C-compatible API** - Universal FFI interface using `extern "C"`
- **Cross-platform builds** - macOS, Linux, Windows, iOS, Android
- **Language bindings** - Rust, Go, Swift, Kotlin
- **Thread-safe** - Safe for concurrent use
- **Memory-safe** - Clear ownership rules with opaque handles

## Quick Start

### Building the Library

```bash
# Clone the repository
git clone --recursive https://github.com/your-repo/openpano-ffi-portable.git
cd openpano-ffi-portable

# Build for current platform
./scripts/build-all.sh macos  # or: linux, ios, android

# Output in dist/ directory
```

### Using from Rust

```rust
use openpano::{Stitcher, Config, StitchMode};

fn main() -> openpano::Result<()> {
    let config = Config::default()
        .mode(StitchMode::EstimateCamera)
        .crop(true);
    
    let mut stitcher = Stitcher::new(config)?;
    stitcher.add_image("photo1.jpg")?;
    stitcher.add_image("photo2.jpg")?;
    
    let panorama = stitcher.stitch()?;
    panorama.save("panorama.jpg", 95)?;
    
    Ok(())
}
```

### Using from Go

```go
package main

import (
    "log"
    "github.com/openpano/go-openpano"
)

func main() {
    config := openpano.DefaultConfig()
    config.Mode = openpano.ModeEstimateCamera
    config.CropResult = true

    stitcher, err := openpano.NewStitcher(config)
    if err != nil {
        log.Fatal(err)
    }
    defer stitcher.Close()

    stitcher.AddImage("photo1.jpg")
    stitcher.AddImage("photo2.jpg")

    result, err := stitcher.Stitch()
    if err != nil {
        log.Fatal(err)
    }
    defer result.Close()

    result.Save("panorama.jpg", 90)
}
```

### Using from Swift (iOS/macOS)

```swift
import OpenPano

let config = StitcherConfig()
    .mode(.estimateCamera)
    .crop(true)

let stitcher = try Stitcher(config: config)
try stitcher.addImage(path: "photo1.jpg")
try stitcher.addImage(path: "photo2.jpg")

let panorama = try stitcher.stitch()
try panorama.save(to: "panorama.jpg", quality: 90)

// Or convert to UIImage
let image = panorama.toUIImage()
```

### Using from Kotlin (Android)

```kotlin
val config = StitcherConfig(
    mode = StitchMode.ESTIMATE_CAMERA,
    cropResult = true
)

Stitcher(config).use { stitcher ->
    stitcher.addImage("photo1.jpg")
    stitcher.addImage("photo2.jpg")
    
    stitcher.stitch().use { panorama ->
        panorama.save("panorama.jpg", quality = 90)
        // Or convert to Bitmap
        val bitmap = panorama.toBitmap()
    }
}
```

## Project Structure

```
openpano-ffi-portable/
├── OpenPano/              # OpenPano submodule
├── ffi/
│   ├── include/
│   │   └── openpano_ffi.h # C header
│   └── src/
│       └── openpano_ffi.cpp
├── bindings/
│   ├── rust/             # Rust crate
│   ├── go/               # Go package
│   ├── swift/            # Swift module
│   └── kotlin/           # Kotlin/JNI
├── scripts/
│   └── build-all.sh      # Build script
├── cmake/                # CMake modules
├── dist/                 # Built libraries
└── docs/                 # Documentation
```

## Stitching Modes

| Mode | Description | Best For |
|------|-------------|----------|
| `CYLINDER` | Cylindrical projection | Standard panoramas (camera rotation) |
| `ESTIMATE_CAMERA` | Bundle adjustment | Arbitrary camera poses |
| `TRANSLATION` | Translation-only | Document scanning, mosaics |

## Build Outputs

| Platform | Output | Location |
|----------|--------|----------|
| macOS | Universal binary | `dist/macos/lib/libopenpano.a` |
| Linux | Static library | `dist/linux/lib/libopenpano.a` |
| iOS | XCFramework | `dist/ios/OpenPano.xcframework` |
| Android | Per-ABI libraries | `dist/android/{abi}/libopenpano.a` |
| Windows | Static library | `dist/windows/lib/libopenpano.a` |

## API Overview

### Core Functions

```c
// Create stitcher
OpenpanoStitcherHandle openpano_stitcher_create(const OpenpanoConfig* config);
void openpano_stitcher_destroy(OpenpanoStitcherHandle handle);

// Add images
OpenpanoError openpano_add_image_file(OpenpanoStitcherHandle handle, const char* path);
OpenpanoError openpano_add_image_data(OpenpanoStitcherHandle handle, 
    const uint8_t* pixels, int32_t width, int32_t height, int32_t stride);

// Execute stitching
OpenpanoError openpano_stitch(OpenpanoStitcherHandle handle, OpenpanoImageHandle* result);

// Handle result
OpenpanoError openpano_image_save(OpenpanoImageHandle image, const char* path, int32_t quality);
void openpano_image_destroy(OpenpanoImageHandle image);
```

### Error Codes

| Code | Meaning |
|------|---------|
| `OPENPANO_OK` | Success |
| `OPENPANO_ERROR_INVALID_HANDLE` | Null or invalid handle |
| `OPENPANO_ERROR_FILE_NOT_FOUND` | Image file not found |
| `OPENPANO_ERROR_INSUFFICIENT_IMAGES` | Need ≥2 images |
| `OPENPANO_ERROR_STITCHING_FAILED` | Stitching failed |
| `OPENPANO_ERROR_OUT_OF_MEMORY` | Memory allocation failed |

## Requirements

### Build Requirements

- CMake 3.20+
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Eigen3

### Optional Dependencies

- libjpeg (for JPEG support)
- OpenMP (for parallel processing)

### Platform-Specific

- **iOS**: Xcode 12+, iOS 13.0+
- **Android**: NDK r21+, API 24+
- **Windows**: MinGW-w64 or MSVC

## License

This project is licensed under the MIT License. OpenPano is also MIT licensed.

## Acknowledgments

- [OpenPano](https://github.com/ppwwyyxx/OpenPano) by Yuxin Wu
- [Eigen](https://eigen.tuxfamily.org/) for linear algebra
- [lodepng](https://lodev.org/lodepng/) for PNG support

