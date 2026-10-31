// Soul Knight Linux — native scene loader
#pragma once

#include "SceneAsset.hpp"

#include <filesystem>
#include <string>

namespace soulknight::platform {
class FileSystem;
}

namespace soulknight::engine::assets {

class SceneLoader {
public:
    static bool Load(const platform::FileSystem& filesystem,
                     const std::filesystem::path& scene_path,
                     SceneAsset& scene,
                     std::string* error = nullptr);
};

} // namespace soulknight::engine::assets
