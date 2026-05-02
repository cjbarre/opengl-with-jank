// bone_mapper.cc - Implementation of bone mapping configuration
#include "bone_mapper.h"

#include <fstream>
#include <iostream>
#include <cmath>

#include "json.hpp"

using json = nlohmann::json;

namespace retarget {

bool BoneMapper::LoadConfig(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << config_path << std::endl;
        return false;
    }

    json config;
    try {
        file >> config;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    }

    m_mappings.clear();

    if (!config.contains("mappings") || !config["mappings"].is_array()) {
        std::cerr << "Config must contain 'mappings' array" << std::endl;
        return false;
    }

    for (const auto& mapping : config["mappings"]) {
        if (!mapping.contains("target") || !mapping["target"].is_string()) {
            std::cerr << "Each mapping must have a 'target' string" << std::endl;
            continue;
        }

        std::string target = mapping["target"].get<std::string>();
        BoneMapping bone_mapping(target);  // Start with unmapped

        // Check for source (can be null, string, or array)
        if (!mapping.contains("source") || mapping["source"].is_null()) {
            // Unmapped bone - use identity transform
            // bone_mapping already initialized as unmapped
        } else if (mapping["source"].is_string()) {
            // Single source bone
            std::string source = mapping["source"].get<std::string>();
            bone_mapping = BoneMapping(target, source);
        } else if (mapping["source"].is_array()) {
            // Multiple source bones (combined)
            std::vector<std::string> sources;
            for (const auto& s : mapping["source"]) {
                if (s.is_string()) {
                    sources.push_back(s.get<std::string>());
                }
            }
            if (!sources.empty()) {
                bone_mapping = BoneMapping(target, sources);
            }
            // else keep as unmapped
        } else {
            std::cerr << "Invalid 'source' type for target: " << target << std::endl;
            // keep as unmapped
        }

        // Check for rotation correction (axis-angle in degrees)
        // Format: "correction_axis_angle": [angle_degrees, axis_x, axis_y, axis_z]
        if (mapping.contains("correction_axis_angle") && mapping["correction_axis_angle"].is_array()) {
            const auto& corr = mapping["correction_axis_angle"];
            if (corr.size() == 4) {
                float angle_deg = corr[0].get<float>();
                float ax = corr[1].get<float>();
                float ay = corr[2].get<float>();
                float az = corr[3].get<float>();

                // Normalize axis
                float axis_len = std::sqrt(ax * ax + ay * ay + az * az);
                if (axis_len > 0.0001f) {
                    ax /= axis_len;
                    ay /= axis_len;
                    az /= axis_len;
                }

                // Convert to quaternion
                float angle_rad = angle_deg * 3.14159265359f / 180.0f;
                float half_angle = angle_rad / 2.0f;
                float s = std::sin(half_angle);
                bone_mapping.correction = ozz::math::Quaternion(
                    ax * s, ay * s, az * s, std::cos(half_angle));
                bone_mapping.has_correction = true;

                std::cout << "  Correction for " << target << ": " << angle_deg
                          << "° around (" << ax << ", " << ay << ", " << az << ")" << std::endl;
            }
        }

        m_mappings.push_back(bone_mapping);
    }

    std::cout << "Loaded " << m_mappings.size() << " bone mappings from " << config_path << std::endl;
    return true;
}

std::optional<BoneMapping> BoneMapper::GetMapping(const std::string& target_bone) const {
    for (const auto& mapping : m_mappings) {
        if (mapping.target_bone == target_bone) {
            return mapping;
        }
    }
    return std::nullopt;
}

void BoneMapper::BuildSourceBoneMap(const std::vector<std::string>& bone_names) {
    m_source_bone_map.clear();
    for (size_t i = 0; i < bone_names.size(); ++i) {
        m_source_bone_map[bone_names[i]] = static_cast<int>(i);
    }
}

void BoneMapper::BuildTargetBoneMap(const std::vector<std::string>& bone_names) {
    m_target_bone_map.clear();
    for (size_t i = 0; i < bone_names.size(); ++i) {
        m_target_bone_map[bone_names[i]] = static_cast<int>(i);
    }
}

int BoneMapper::GetSourceBoneIndex(const std::string& name) const {
    auto it = m_source_bone_map.find(name);
    return it != m_source_bone_map.end() ? it->second : -1;
}

int BoneMapper::GetTargetBoneIndex(const std::string& name) const {
    auto it = m_target_bone_map.find(name);
    return it != m_target_bone_map.end() ? it->second : -1;
}

ozz::math::Quaternion BoneMapper::CombineRotations(
    const std::vector<ozz::math::Quaternion>& rotations) {
    if (rotations.empty()) {
        return ozz::math::Quaternion::identity();
    }

    ozz::math::Quaternion result = rotations[0];
    for (size_t i = 1; i < rotations.size(); ++i) {
        // Quaternion multiplication: result = result * rotations[i]
        const auto& a = result;
        const auto& b = rotations[i];
        result.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
        result.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
        result.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
        result.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    }

    // Normalize the result
    float len = std::sqrt(result.x * result.x + result.y * result.y +
                          result.z * result.z + result.w * result.w);
    if (len > 0.0001f) {
        result.x /= len;
        result.y /= len;
        result.z /= len;
        result.w /= len;
    }

    return result;
}

}  // namespace retarget
