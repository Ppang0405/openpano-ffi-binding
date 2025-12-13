# OpenPano Patches

This directory contains patches that are automatically applied to the OpenPano submodule during build.

## Patches

### 1. `flann-cpp17-compat.patch`
**Purpose**: Fix C++17 compatibility issues in FLANN library

**Changes**:
- `src/third-party/flann/util/heap.h`: Replace deprecated `std::binary_function` with explicit type definitions
- `src/third-party/flann/util/random.h`: Replace deprecated `std::random_shuffle` with `std::shuffle` + `std::mt19937`
- `src/third-party/flann/algorithms/kdtree_index.h`: Replace `std::random_shuffle` with `std::shuffle` + `std::mt19937`
- `src/third-party/flann/util/lsh_table.h`: Replace `std::random_shuffle` with `std::shuffle` + `std::mt19937`

**Why needed**: C++17 removed `std::binary_function` and `std::random_shuffle`, causing compilation failures with modern compilers.

### 2. `imageref-dims-cache.patch`
**Purpose**: Fix image dimension caching for lazy loading

**Changes**:
- `src/stitch/imageref.hh`: 
  - Add `dims_cached` flag to track whether dimensions have been loaded
  - Initialize `_width`, `_height`, and `dims_cached` in constructor
  - Make `width()` and `height()` load image on-demand if dimensions not cached
  - Keep dimensions valid after `release()` to avoid division by zero

**Why needed**: When images are released but dimensions are queried (e.g., in `get_final_resolution()`), uninitialized width/height caused division by zero, resulting in infinite resolution values.

## How Patches Are Applied

The build script (`scripts/build-all.sh`) automatically applies all patches in this directory before building:

```bash
apply_patches() {
    for patch in patches/*.patch; do
        git -C OpenPano apply "$patch"
    done
}
```

This keeps the OpenPano submodule clean while applying necessary fixes for FFI and modern C++ compatibility.

## Creating New Patches

To create a new patch:

```bash
# Make changes in OpenPano submodule
cd OpenPano
# ... edit files ...

# Create patch
git diff > ../patches/my-new-fix.patch

# Test it
git checkout -- .
git apply ../patches/my-new-fix.patch
```

## Reverting Patches

To reset OpenPano to original state:

```bash
cd OpenPano
git checkout -- .
git clean -fd
```

