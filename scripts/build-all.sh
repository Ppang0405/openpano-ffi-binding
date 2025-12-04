#!/bin/bash
# =============================================================================
# OpenPano FFI Cross-Platform Build Script
# =============================================================================
#
# Usage:
#   ./scripts/build-all.sh [platform]
#
# Platforms:
#   macos     - Build for macOS (Universal binary: x64 + arm64)
#   linux     - Build for Linux
#   ios       - Build for iOS (device + simulator)
#   android   - Build for Android (all ABIs)
#   windows   - Build for Windows (MinGW cross-compile)
#   all       - Build for macOS + iOS + Android
#
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
OUTPUT_DIR="$PROJECT_ROOT/dist"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_step() { echo -e "${BLUE}[STEP]${NC} $1"; }

# Check for required tools
check_requirements() {
    if ! command -v cmake &> /dev/null; then
        log_error "cmake is required but not found"
        exit 1
    fi
}

# Apply patches to submodules (C++17 compatibility fixes for FLANN)
apply_patches() {
    local PATCH_DIR="$PROJECT_ROOT/patches"
    local OPENPANO_DIR="$PROJECT_ROOT/OpenPano"
    
    if [ ! -d "$PATCH_DIR" ]; then
        return 0
    fi
    
    for patch in "$PATCH_DIR"/*.patch; do
        if [ -f "$patch" ]; then
            patch_name=$(basename "$patch")
            
            # Check if patch is already applied by testing reverse apply
            if git -C "$OPENPANO_DIR" apply --reverse --check "$patch" 2>/dev/null; then
                log_info "Patch already applied: $patch_name"
            else
                log_step "Applying patch: $patch_name"
                if git -C "$OPENPANO_DIR" apply --check "$patch" 2>/dev/null; then
                    git -C "$OPENPANO_DIR" apply "$patch"
                    log_info "Patch applied successfully: $patch_name"
                else
                    log_warn "Patch may have conflicts or is partially applied: $patch_name"
                fi
            fi
        fi
    done
}

# Build for macOS (Universal binary: x64 + arm64)
build_macos() {
    log_info "Building for macOS (Universal binary)..."
    
    # x86_64 build
    log_step "Building x86_64..."
    cmake -B "$BUILD_DIR/macos-x64" \
        -DCMAKE_OSX_ARCHITECTURES=x86_64 \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        "$PROJECT_ROOT"
    cmake --build "$BUILD_DIR/macos-x64" --parallel
    
    # arm64 build
    log_step "Building arm64..."
    cmake -B "$BUILD_DIR/macos-arm64" \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        "$PROJECT_ROOT"
    cmake --build "$BUILD_DIR/macos-arm64" --parallel
    
    # Create universal binary
    log_step "Creating universal binary..."
    mkdir -p "$OUTPUT_DIR/macos/lib"
    mkdir -p "$OUTPUT_DIR/macos/include"
    
    lipo -create \
        "$BUILD_DIR/macos-x64/libopenpano_all.a" \
        "$BUILD_DIR/macos-arm64/libopenpano_all.a" \
        -output "$OUTPUT_DIR/macos/lib/libopenpano.a"
    
    # Copy header
    cp "$PROJECT_ROOT/ffi/include/openpano_ffi.h" "$OUTPUT_DIR/macos/include/"
    
    log_info "macOS build complete: $OUTPUT_DIR/macos/"
}

# Build for Linux
build_linux() {
    log_info "Building for Linux..."
    
    cmake -B "$BUILD_DIR/linux" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        "$PROJECT_ROOT"
    cmake --build "$BUILD_DIR/linux" --parallel
    
    mkdir -p "$OUTPUT_DIR/linux/lib"
    mkdir -p "$OUTPUT_DIR/linux/include"
    cp "$BUILD_DIR/linux/libopenpano_all.a" "$OUTPUT_DIR/linux/lib/libopenpano.a"
    cp "$PROJECT_ROOT/ffi/include/openpano_ffi.h" "$OUTPUT_DIR/linux/include/"
    
    log_info "Linux build complete: $OUTPUT_DIR/linux/"
}

# Build for iOS
build_ios() {
    log_info "Building for iOS..."
    
    if [[ "$(uname)" != "Darwin" ]]; then
        log_error "iOS builds require macOS"
        exit 1
    fi
    
    # Find Eigen3 from Homebrew (header-only, works for cross-compile)
    EIGEN3_DIR=""
    if [ -d "/opt/homebrew/share/eigen3/cmake" ]; then
        EIGEN3_DIR="/opt/homebrew/share/eigen3/cmake"
    elif [ -d "/usr/local/share/eigen3/cmake" ]; then
        EIGEN3_DIR="/usr/local/share/eigen3/cmake"
    else
        log_error "Eigen3 not found. Install with: brew install eigen"
        exit 1
    fi
    log_info "Using Eigen3 from: $EIGEN3_DIR"
    
    # Device (arm64)
    log_step "Building iOS device (arm64)..."
    cmake -B "$BUILD_DIR/ios-device" \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DOPENPANO_USE_OPENMP=OFF \
        -DEigen3_DIR="$EIGEN3_DIR" \
        "$PROJECT_ROOT"
    cmake --build "$BUILD_DIR/ios-device" --parallel
    
    # Simulator (x86_64 + arm64)
    log_step "Building iOS simulator..."
    cmake -B "$BUILD_DIR/ios-sim" \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_SYSROOT=iphonesimulator \
        -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DOPENPANO_USE_OPENMP=OFF \
        -DEigen3_DIR="$EIGEN3_DIR" \
        "$PROJECT_ROOT"
    cmake --build "$BUILD_DIR/ios-sim" --parallel
    
    # Create XCFramework
    log_step "Creating XCFramework..."
    mkdir -p "$OUTPUT_DIR/ios"
    
    # Remove existing framework if present
    rm -rf "$OUTPUT_DIR/ios/OpenPano.xcframework"
    
    xcodebuild -create-xcframework \
        -library "$BUILD_DIR/ios-device/libopenpano_all.a" \
        -headers "$PROJECT_ROOT/ffi/include" \
        -library "$BUILD_DIR/ios-sim/libopenpano_all.a" \
        -headers "$PROJECT_ROOT/ffi/include" \
        -output "$OUTPUT_DIR/ios/OpenPano.xcframework"
    
    log_info "iOS build complete: $OUTPUT_DIR/ios/OpenPano.xcframework"
}

# Build for Android
build_android() {
    log_info "Building for Android..."
    
    if [ -z "$ANDROID_NDK" ]; then
        # Try to find NDK
        if [ -d "$ANDROID_HOME/ndk" ]; then
            ANDROID_NDK=$(ls -d "$ANDROID_HOME/ndk"/* 2>/dev/null | head -1)
        elif [ -d "$HOME/Library/Android/sdk/ndk" ]; then
            ANDROID_NDK=$(ls -d "$HOME/Library/Android/sdk/ndk"/* 2>/dev/null | head -1)
        fi
        
        if [ -z "$ANDROID_NDK" ]; then
            log_error "ANDROID_NDK environment variable not set and NDK not found"
            log_error "Set it to your NDK path, e.g.: export ANDROID_NDK=/path/to/ndk"
            exit 1
        fi
        log_info "Using Android NDK: $ANDROID_NDK"
    fi
    
    # Find Eigen3 (header-only, works for cross-compile)
    EIGEN3_DIR=""
    if [ -d "/opt/homebrew/share/eigen3/cmake" ]; then
        EIGEN3_DIR="/opt/homebrew/share/eigen3/cmake"
    elif [ -d "/usr/local/share/eigen3/cmake" ]; then
        EIGEN3_DIR="/usr/local/share/eigen3/cmake"
    elif [ -d "/usr/share/eigen3/cmake" ]; then
        EIGEN3_DIR="/usr/share/eigen3/cmake"
    else
        log_warn "Eigen3 cmake config not found, trying default search paths"
    fi
    
    ABIS=("armeabi-v7a" "arm64-v8a" "x86" "x86_64")
    
    for ABI in "${ABIS[@]}"; do
        log_step "Building for Android ABI: $ABI"
        
        EIGEN_ARG=""
        if [ -n "$EIGEN3_DIR" ]; then
            EIGEN_ARG="-DEigen3_DIR=$EIGEN3_DIR"
        fi
        
        cmake -B "$BUILD_DIR/android-$ABI" \
            -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
            -DANDROID_ABI=$ABI \
            -DANDROID_PLATFORM=android-24 \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_SHARED_LIBS=OFF \
            -DOPENPANO_USE_OPENMP=OFF \
            $EIGEN_ARG \
            "$PROJECT_ROOT"
        cmake --build "$BUILD_DIR/android-$ABI" --parallel
        
        mkdir -p "$OUTPUT_DIR/android/$ABI"
        cp "$BUILD_DIR/android-$ABI/libopenpano_all.a" "$OUTPUT_DIR/android/$ABI/libopenpano.a"
    done
    
    # Copy header
    mkdir -p "$OUTPUT_DIR/android/include"
    cp "$PROJECT_ROOT/ffi/include/openpano_ffi.h" "$OUTPUT_DIR/android/include/"
    
    log_info "Android build complete: $OUTPUT_DIR/android/"
}

# Build for Windows (MinGW cross-compile)
build_windows() {
    log_info "Building for Windows (MinGW)..."
    
    if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        log_error "MinGW cross-compiler not found"
        log_error "Install with: brew install mingw-w64 (macOS) or apt install mingw-w64 (Linux)"
        exit 1
    fi
    
    cmake -B "$BUILD_DIR/windows" \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DOPENPANO_USE_OPENMP=OFF \
        "$PROJECT_ROOT"
    cmake --build "$BUILD_DIR/windows" --parallel
    
    mkdir -p "$OUTPUT_DIR/windows/lib"
    mkdir -p "$OUTPUT_DIR/windows/include"
    cp "$BUILD_DIR/windows/libopenpano_all.a" "$OUTPUT_DIR/windows/lib/libopenpano.a"
    cp "$PROJECT_ROOT/ffi/include/openpano_ffi.h" "$OUTPUT_DIR/windows/include/"
    
    log_info "Windows build complete: $OUTPUT_DIR/windows/"
}

# Clean build artifacts
clean() {
    log_info "Cleaning build artifacts..."
    rm -rf "$BUILD_DIR"
    rm -rf "$OUTPUT_DIR"
    log_info "Clean complete"
}

# Print usage
usage() {
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  macos     Build for macOS (Universal binary)"
    echo "  linux     Build for Linux"
    echo "  ios       Build for iOS (XCFramework)"
    echo "  android   Build for Android (all ABIs)"
    echo "  windows   Build for Windows (MinGW)"
    echo "  all       Build macOS + iOS + Android"
    echo "  clean     Remove build artifacts"
    echo ""
}

# Main
check_requirements
apply_patches

case "${1:-all}" in
    macos)
        build_macos
        ;;
    linux)
        build_linux
        ;;
    ios)
        build_ios
        ;;
    android)
        build_android
        ;;
    windows)
        build_windows
        ;;
    all)
        build_macos
        build_ios
        build_android
        ;;
    clean)
        clean
        ;;
    help|--help|-h)
        usage
        ;;
    *)
        log_error "Unknown command: $1"
        usage
        exit 1
        ;;
esac

echo ""
log_info "Build complete! Output in: $OUTPUT_DIR"

