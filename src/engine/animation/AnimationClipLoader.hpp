// Soul Knight Linux — native animation clip loader
#pragma once

#include "AnimationClip.hpp"

#include <filesystem>
#include <string>

namespace soulknight::platform {
class FileSystem;
}

namespace soulknight::engine::animation {

class AnimationClipLoader {
public:
    static bool Load(const platform::FileSystem& filesystem,
                     const std::filesystem::path& path,
                     AnimationClip& clip,
                     std::string* error = nullptr);

    // Parses already-read native animation JSON. Exposed for deterministic
    // unit tests and for callers that already resolved the asset path.
    static bool Parse(const std::string& text,
                      AnimationClip& clip,
                      std::string* error = nullptr);
};

} // namespace soulknight::engine::animation
