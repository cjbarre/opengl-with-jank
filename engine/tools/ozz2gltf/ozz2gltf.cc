// ozz2gltf - Convert ozz-animation skeleton files to glTF format
//
// Usage:
//   ozz2gltf input.ozz output.gltf    # JSON format
//   ozz2gltf input.ozz output.glb     # Binary format

#include <iostream>
#include <string>

#include "ozz/animation/runtime/skeleton.h"
#include "skeleton_converter.h"

void PrintUsage(const char* program) {
    std::cout << "Usage: " << program << " <input.ozz> <output.gltf|.glb>\n\n";
    std::cout << "Convert ozz-animation skeleton to glTF format for use in\n";
    std::cout << "external 3D software (Blender, Maya, etc.).\n\n";
    std::cout << "Output format is determined by file extension:\n";
    std::cout << "  .gltf  - JSON format (human-readable)\n";
    std::cout << "  .glb   - Binary format (smaller file size)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        PrintUsage(argv[0]);
        return argc == 1 ? 0 : 1;
    }

    std::string input_path = argv[1];
    std::string output_path = argv[2];

    // Check for help flag
    if (input_path == "-h" || input_path == "--help") {
        PrintUsage(argv[0]);
        return 0;
    }

    // Validate output extension
    bool valid_ext = false;
    if (output_path.size() >= 5) {
        std::string ext = output_path.substr(output_path.size() - 5);
        if (ext == ".gltf") valid_ext = true;
    }
    if (output_path.size() >= 4) {
        std::string ext = output_path.substr(output_path.size() - 4);
        if (ext == ".glb") valid_ext = true;
    }

    if (!valid_ext) {
        std::cerr << "Error: Output file must have .gltf or .glb extension\n";
        return 1;
    }

    // Load skeleton
    ozz::animation::Skeleton skeleton;
    if (!skeleton_converter::LoadSkeleton(input_path, &skeleton)) {
        return 1;
    }

    std::cout << "Loaded skeleton: " << input_path << std::endl;
    std::cout << "  Joints: " << skeleton.num_joints() << std::endl;

    // Print joint names
    const auto& names = skeleton.joint_names();
    std::cout << "  Joint hierarchy:" << std::endl;
    const auto& parents = skeleton.joint_parents();
    for (int i = 0; i < skeleton.num_joints(); i++) {
        int depth = 0;
        int p = parents[i];
        while (p != ozz::animation::Skeleton::kNoParent) {
            depth++;
            p = parents[p];
        }
        std::cout << "    ";
        for (int d = 0; d < depth; d++) std::cout << "  ";
        std::cout << names[i] << std::endl;
    }

    // Convert to glTF
    if (!skeleton_converter::ConvertToGltf(skeleton, output_path)) {
        return 1;
    }

    return 0;
}
