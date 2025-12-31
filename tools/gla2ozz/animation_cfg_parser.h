// Animation.cfg parser for Jedi Academy
// Parses animation clip definitions from animation.cfg files

#ifndef ANIMATION_CFG_PARSER_H_
#define ANIMATION_CFG_PARSER_H_

#include <string>
#include <vector>

namespace gla {

// Animation clip definition from animation.cfg
struct AnimationClip {
    std::string name;      // Animation name (e.g., "BOTH_STAND1")
    int start_frame;       // First frame in GLA
    int frame_count;       // Number of frames
    int loop_frame;        // Frame to loop back to (-1 = no loop)
    float fps;             // Playback speed (frames per second)

    // Calculated properties
    float GetDuration() const {
        if (fps > 0.0f && frame_count > 1) {
            return static_cast<float>(frame_count - 1) / fps;
        }
        return 0.0f;
    }
};

class AnimationCfgParser {
 public:
    AnimationCfgParser();
    ~AnimationCfgParser();

    // Load animation.cfg from a specific path
    bool Load(const char* cfg_path);

    // Load animation.cfg from the same directory as a GLA file
    // Looks for "animation.cfg" in the same directory as the GLA
    bool LoadFromGlaDirectory(const char* gla_path);

    // Accessors
    const std::vector<AnimationClip>& GetClips() const { return m_clips; }
    size_t GetClipCount() const { return m_clips.size(); }

    // Find a clip by name (returns nullptr if not found)
    const AnimationClip* FindClip(const std::string& name) const;

    // Get all clip names (for OzzImporter::GetAnimationNames)
    std::vector<std::string> GetClipNames() const;

 private:
    bool ParseLine(const std::string& line);

    std::vector<AnimationClip> m_clips;
};

}  // namespace gla

#endif  // ANIMATION_CFG_PARSER_H_
