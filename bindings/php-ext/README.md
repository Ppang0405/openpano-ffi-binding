# OpenPano PHP Extension

Native PHP extension for panorama image stitching built with ext-php-rs.

## Requirements

- PHP 8.1 or later (tested with PHP 8.4)
- Rust 1.70 or later
- cargo-php: `cargo install cargo-php --locked`

## Building for PHP 8.4

```bash
# Install cargo-php if you haven't already
cargo install cargo-php --locked

# Build the extension
cargo build --release

# Install to your PHP
cargo php install --release
```

## Verify Installation

```bash
php -m | grep openpano
```

Should output: `openpano`

## Usage

### Simple API (One Function Call)

```php
<?php

// Stitch images in one call
openpano_stitch_images(
    ['photo1.jpg', 'photo2.jpg', 'photo3.jpg'],
    'panorama.jpg',
    95,    // quality (optional, default: 90)
    true   // crop borders (optional, default: true)
);
```

### Object-Oriented API (More Control)

```php
<?php

// Create configuration
$config = new StitcherConfig();
$config->mode = 1;  // 0=cylinder, 1=estimate_camera
$config->crop = true;
$config->straighten = true;

// Create stitcher
$stitcher = new Stitcher($config);

// Add images
$stitcher->addImage('/path/to/photo1.jpg');
$stitcher->addImage('/path/to/photo2.jpg');
$stitcher->addImage('/path/to/photo3.jpg');

// Stitch and save
if ($stitcher->stitch('panorama.jpg', 95)) {
    echo "Panorama created successfully!\n";
    echo "Total images: " . $stitcher->getImageCount() . "\n";
}
```

### Web Application Example

```php
<?php

// upload.php - Handle image uploads
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_FILES['images'])) {
    $uploadDir = '/tmp/panorama/' . uniqid();
    mkdir($uploadDir, 0755, true);
    
    $imagePaths = [];
    foreach ($_FILES['images']['tmp_name'] as $key => $tmpName) {
        $filename = $uploadDir . '/' . basename($_FILES['images']['name'][$key]);
        move_uploaded_file($tmpName, $filename);
        $imagePaths[] = $filename;
    }
    
    $outputPath = $uploadDir . '/panorama.jpg';
    
    try {
        openpano_stitch_images($imagePaths, $outputPath, 90, true);
        
        // Serve the result
        header('Content-Type: image/jpeg');
        readfile($outputPath);
        
        // Cleanup
        array_map('unlink', $imagePaths);
        unlink($outputPath);
        rmdir($uploadDir);
    } catch (Exception $e) {
        http_response_code(500);
        echo json_encode(['error' => $e->getMessage()]);
    }
}
```

## Generating IDE Stubs

```bash
cargo php stubs --stdout > openpano.php
```

This creates a stub file with full PHPDoc for IDE autocomplete.

## Distribution

### Build for Your Server

```bash
# Build release version
cargo build --release

# Extension will be at:
# target/release/libopenpano_php.dylib (macOS)
# target/release/libopenpano_php.so (Linux)
```

### Install on Production Server

```bash
# Copy to PHP extensions directory
sudo cp target/release/libopenpano_php.so $(php-config --extension-dir)/openpano.so

# Enable extension
echo "extension=openpano.so" | sudo tee /etc/php/8.4/mods-available/openpano.ini
sudo phpenmod openpano

# Restart PHP-FPM
sudo systemctl restart php8.4-fpm
```

## Performance

- **10-100x faster** than PHP FFI
- **Direct memory access** - no serialization overhead
- **Zero-copy** where possible
- **Type-safe** - Rust prevents segfaults

## Troubleshooting

### Extension not loading

```bash
# Check PHP can find it
php -d extension=openpano.so -m | grep openpano

# Check for errors
php -d extension=openpano.so -r "var_dump(extension_loaded('openpano'));"
```

### Build errors

Make sure you have the OpenPano library built first:

```bash
cd ../..
./scripts/build-all.sh macos
```

