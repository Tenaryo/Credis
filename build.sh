#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
BUILD_TYPE="${1:-Debug}"

# Purge stale CMake cache when switching build types to prevent
# CMAKE_BUILD_TYPE from being silently empty (defaults to -O0).
if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cached_type=$(grep 'CMAKE_BUILD_TYPE:STRING=' "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2)
    if [[ "$cached_type" != "$BUILD_TYPE" ]]; then
        echo "Build type switched ($cached_type -> $BUILD_TYPE), purging cache"
        rm -f "$BUILD_DIR/CMakeCache.txt"
    fi
fi

cmake -B "$BUILD_DIR" -S . -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$BUILD_DIR" -j$(nproc)
