#!/bin/bash
# Build script for OpenPano PHP extension

set -e

echo "🔨 Building OpenPano PHP Extension for PHP 8.4..."

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

# Check PHP version
PHP_VERSION=$(php -r "echo PHP_MAJOR_VERSION . '.' . PHP_MINOR_VERSION;")
echo -e "${BLUE}Detected PHP version: $PHP_VERSION${NC}"

if [ "$PHP_VERSION" != "8.4" ]; then
    echo "⚠️  Warning: This was built for PHP 8.4, you're running $PHP_VERSION"
    echo "The extension may still work, but recompilation is recommended."
fi

# Build
echo "📦 Building extension..."
cargo build --release

# Get extension info
EXT_DIR=$(php-config --extension-dir)
EXT_FILE="target/release/libopenpano_php.$(if [[ "$OSTYPE" == "darwin"* ]]; then echo "dylib"; else echo "so"; fi)"

echo -e "${GREEN}✅ Build complete!${NC}"
echo ""
echo "Extension file: $EXT_FILE"
echo "PHP extensions directory: $EXT_DIR"
echo ""
echo "To install:"
echo "  cargo php install --release"
echo ""
echo "Or manually:"
echo "  sudo cp $EXT_FILE $EXT_DIR/openpano.so"
echo "  echo 'extension=openpano.so' | sudo tee /etc/php/8.4/mods-available/openpano.ini"

