#!/bin/bash
# Build script for gpSP PSP standalone

echo "================================================"
echo "  Building gpSP for PSP"
echo "================================================"
echo ""

# Check if PSPSDK is installed
if ! command -v psp-gcc &> /dev/null; then
    echo "ERROR: psp-gcc not found!"
    echo "Please install PSPSDK and add it to your PATH"
    exit 1
fi

echo "PSPSDK found: $(psp-gcc --version | head -n1)"
echo ""

# Check required tools
echo "Checking required tools..."
REQUIRED_TOOLS="psp-gcc psp-g++ psp-ar psp-config mksfo pack-pbp psp-fixup-imports psp-strip"
MISSING_TOOLS=""

for tool in $REQUIRED_TOOLS; do
    if ! command -v $tool &> /dev/null; then
        MISSING_TOOLS="$MISSING_TOOLS $tool"
    fi
done

if [ ! -z "$MISSING_TOOLS" ]; then
    echo "ERROR: Missing required tools:$MISSING_TOOLS"
    exit 1
fi

echo "All tools found!"
echo ""

# Clean previous build
echo "Cleaning previous build..."
make -f Makefile.psp clean
echo ""

# Build
echo "Building gpSP..."
make -f Makefile.psp -j$(nproc 2>/dev/null || echo 2)

# Check if build succeeded
if [ -f "EBOOT.PBP" ]; then
    echo ""
    echo "================================================"
    echo "  Build successful!"
    echo "================================================"
    echo ""
    echo "Output files:"
    echo "  - EBOOT.PBP (Copy this to ms0:/PSP/GAME/gpSP/)"
    echo ""
    echo "Installation:"
    echo "  1. Copy EBOOT.PBP to ms0:/PSP/GAME/gpSP/"
    echo "  2. Create ms0:/PSP/GAME/gpSP/ROMS/"
    echo "  3. Put your .gba ROMs in the ROMS folder"
    echo "  4. Run gpSP from the PSP Game menu"
    echo ""
else
    echo ""
    echo "================================================"
    echo "  Build failed!"
    echo "================================================"
    echo ""
    echo "Please check the error messages above."
    exit 1
fi

