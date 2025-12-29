#!/bin/bash
set -e
DIR="$(cd "$(dirname "$0")/.." && pwd)"
OZZ_DIR="$DIR/third_party/ozz-animation"

# Ensure submodule is initialized
git submodule update --init "$OZZ_DIR"

# Build conversion tools
mkdir -p "$OZZ_DIR/build"
cd "$OZZ_DIR/build"
cmake .. -DBUILD_SHARED_LIBS=OFF -Dozz_build_samples=OFF -Dozz_build_howtos=OFF -Dozz_build_tests=OFF
make -j$(sysctl -n hw.ncpu) fbx2ozz gltf2ozz

echo "Tools built at:"
echo "  $OZZ_DIR/build/src/animation/offline/fbx/fbx2ozz"
echo "  $OZZ_DIR/build/src/animation/offline/gltf/gltf2ozz"
