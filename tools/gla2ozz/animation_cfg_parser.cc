// Animation.cfg parser implementation

#include "animation_cfg_parser.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

#include "ozz/base/log.h"

namespace gla {

AnimationCfgParser::AnimationCfgParser() {
}

AnimationCfgParser::~AnimationCfgParser() {
}

bool AnimationCfgParser::Load(const char* cfg_path) {
    std::ifstream file(cfg_path);
    if (!file.is_open()) {
        ozz::log::Err() << "Failed to open animation.cfg: " << cfg_path << std::endl;
        return false;
    }

    m_clips.clear();

    std::string line;
    int line_num = 0;
    while (std::getline(file, line)) {
        ++line_num;

        // Skip empty lines and comments
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) {
            continue;  // Empty line
        }

        if (line[start] == '/' && start + 1 < line.length() && line[start + 1] == '/') {
            continue;  // Comment line
        }

        if (!ParseLine(line)) {
            ozz::log::LogV() << "Skipping line " << line_num << ": " << line << std::endl;
        }
    }

    ozz::log::LogV() << "Loaded " << m_clips.size() << " animation clips from "
                     << cfg_path << std::endl;

    return !m_clips.empty();
}

bool AnimationCfgParser::LoadFromGlaDirectory(const char* gla_path) {
    std::string path(gla_path);

    // Find the directory part
    size_t last_sep = path.find_last_of("/\\");
    std::string dir;
    if (last_sep != std::string::npos) {
        dir = path.substr(0, last_sep + 1);
    } else {
        dir = "./";
    }

    // Try to load animation.cfg from the same directory
    std::string cfg_path = dir + "animation.cfg";
    return Load(cfg_path.c_str());
}

bool AnimationCfgParser::ParseLine(const std::string& line) {
    // Format: name  startFrame  frameCount  loopFrame  fps
    // Example: BOTH_STAND1      0      40      -1      20

    std::istringstream iss(line);

    AnimationClip clip;
    int loop_frame;
    float fps;

    // Read name
    if (!(iss >> clip.name)) {
        return false;
    }

    // Skip if name starts with "//" (inline comment)
    if (clip.name.length() >= 2 && clip.name[0] == '/' && clip.name[1] == '/') {
        return false;
    }

    // Read numeric values
    if (!(iss >> clip.start_frame >> clip.frame_count >> loop_frame >> fps)) {
        return false;
    }

    clip.loop_frame = loop_frame;
    clip.fps = std::abs(fps);  // FPS can be negative in some entries, use absolute value

    // Validate
    if (clip.frame_count <= 0) {
        return false;
    }

    if (clip.start_frame < 0) {
        return false;
    }

    m_clips.push_back(clip);
    return true;
}

const AnimationClip* AnimationCfgParser::FindClip(const std::string& name) const {
    for (const auto& clip : m_clips) {
        if (clip.name == name) {
            return &clip;
        }
    }
    return nullptr;
}

std::vector<std::string> AnimationCfgParser::GetClipNames() const {
    std::vector<std::string> names;
    names.reserve(m_clips.size());
    for (const auto& clip : m_clips) {
        names.push_back(clip.name);
    }
    return names;
}

}  // namespace gla
