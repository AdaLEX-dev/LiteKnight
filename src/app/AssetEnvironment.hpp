// Soul Knight Linux — local reference/native asset environment report
#pragma once

#include <filesystem>
#include <string>

namespace soulknight::platform {
class FileSystem;
}

namespace soulknight::app {

struct AssetEnvironmentReport {
    std::filesystem::path project_root;
    std::filesystem::path reference_apk;
    std::filesystem::path extraction_root;
    std::filesystem::path generated_root;
    std::filesystem::path runtime_scene;
    std::filesystem::path inventory_path;
    std::filesystem::path texture_inventory_path;
    std::filesystem::path reference_catalogue_path;
    std::string scene_sha256;
    std::string reference_apk_sha256;
    std::string asset_inventory_sha256;
    bool reference_found = false;
    bool generated_found = false;
    bool inventory_found = false;
    bool reference_catalogue_found = false;
    int integrated_textures = 0;
    int integrated_sprites = 0;
    int integrated_characters = 0;
    int integrated_weapons = 0;
    int integrated_enemies = 0;
    int reference_textures = 0;
    int reference_sprites = 0;
    int reference_weapons = 0;
    int reference_enemies = 0;
    int reference_animations = 0;
};

AssetEnvironmentReport DetectAssetEnvironment(
    const platform::FileSystem& filesystem);

void LogAssetEnvironment(const AssetEnvironmentReport& report);

} // namespace soulknight::app
