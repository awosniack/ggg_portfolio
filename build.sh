#!/bin/bash


set -e

echo "=== Building Inventory System ==="

# Create build directory
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# Configure with CMake
echo "Configuring project..."
cmake ..

# Build
echo "Building..."
cmake --build . -j$(nproc)

echo ""
echo "=== Build Complete ==="
echo "Server executable: build/server/server"
echo "Client executable: build/client/client"
echo ""
echo "To run:"
echo "  Terminal 1: ./build/server/server"
echo "  Terminal 2: ./build/client/client"
