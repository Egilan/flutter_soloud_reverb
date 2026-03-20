#!/bin/bash
# Build libpd as static libraries for iOS (device + simulator).
# Outputs to ios/libs/libpd_iOS-device.a and ios/libs/libpd_iOS-simulator.a
# Also copies public headers to ios/include/libpd/

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LIBPD_DIR="$ROOT_DIR/third_party/libpd"
BUILD_DIR="$ROOT_DIR/build_libpd_ios"
OUTPUT_DIR="$ROOT_DIR/ios/libs"
HEADER_DIR="$ROOT_DIR/ios/include/libpd"
DEPLOYMENT_TARGET="13.0"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/device" "$BUILD_DIR/simulator" "$OUTPUT_DIR" "$HEADER_DIR"

CMAKE_COMMON_FLAGS=(
  -DLIBPD_STATIC=ON
  -DLIBPD_SHARED=OFF
  -DPD_EXTRA=ON
  -DPD_UTILS=ON
  -DPD_MULTI=OFF
  -DCMAKE_SYSTEM_NAME=iOS
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOYMENT_TARGET"
)

echo "=== Building libpd for iOS device (arm64) ==="
cmake -S "$LIBPD_DIR" -B "$BUILD_DIR/device" \
  "${CMAKE_COMMON_FLAGS[@]}" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_SYSROOT=iphoneos
cmake --build "$BUILD_DIR/device" --target libpd_static --config Release -- -j$(sysctl -n hw.ncpu)

echo "=== Building libpd for iOS simulator (arm64;x86_64) ==="
cmake -S "$LIBPD_DIR" -B "$BUILD_DIR/simulator" \
  "${CMAKE_COMMON_FLAGS[@]}" \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_SYSROOT=iphonesimulator
cmake --build "$BUILD_DIR/simulator" --target libpd_static --config Release -- -j$(sysctl -n hw.ncpu)

echo "=== Copying libraries ==="
# CMake output name is "pd" on non-Windows, so the file is libpd.a
find "$BUILD_DIR/device" -name "libpd.a" | head -1 | xargs -I{} cp {} "$OUTPUT_DIR/libpd_iOS-device.a"
find "$BUILD_DIR/simulator" -name "libpd.a" | head -1 | xargs -I{} cp {} "$OUTPUT_DIR/libpd_iOS-simulator.a"

echo "=== Copying headers ==="
cp "$LIBPD_DIR/libpd_wrapper/z_libpd.h" "$HEADER_DIR/"
cp "$LIBPD_DIR/pure-data/src/m_pd.h" "$HEADER_DIR/"

echo "=== Cleaning up build directory ==="
rm -rf "$BUILD_DIR"

echo "=== Done ==="
echo "Libraries: $OUTPUT_DIR/libpd_iOS-{device,simulator}.a"
echo "Headers:   $HEADER_DIR/"
ls -lh "$OUTPUT_DIR"/libpd_iOS-*.a
