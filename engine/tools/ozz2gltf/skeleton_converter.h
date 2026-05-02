// Skeleton converter: ozz-animation skeleton to glTF format

#ifndef SKELETON_CONVERTER_H_
#define SKELETON_CONVERTER_H_

#include <string>

#include "ozz/animation/runtime/skeleton.h"

namespace skeleton_converter {

// Convert an ozz skeleton to glTF format.
// Output format is determined by extension: .gltf (JSON) or .glb (binary).
// Returns true on success.
bool ConvertToGltf(const ozz::animation::Skeleton& skeleton,
                   const std::string& output_path);

// Load skeleton from .ozz file.
// Returns true on success.
bool LoadSkeleton(const std::string& path, ozz::animation::Skeleton* skeleton);

}  // namespace skeleton_converter

#endif  // SKELETON_CONVERTER_H_
