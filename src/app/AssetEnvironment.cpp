// Soul Knight Linux — local reference/native asset environment report
#include "AssetEnvironment.hpp"

#include "engine/assets/SceneAsset.hpp"
#include "engine/core/Log.hpp"
#include "platform/filesystem/FileSystem.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace soulknight::app {
namespace {

std::filesystem::path FindProjectRoot() {
    if (const char* configured = std::getenv("LITEKNIGHT_PROJECT_ROOT");
        configured != nullptr && *configured != '\0') {
        return std::filesystem::path(configured);
    }
    std::error_code error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    if (!error) {
        for (std::filesystem::path candidate = current;
             !candidate.empty(); candidate = candidate.parent_path()) {
            if (std::filesystem::is_regular_file(candidate / "VERSION", error) &&
                std::filesystem::is_directory(candidate / "analysis", error)) {
                return candidate;
            }
            if (candidate == candidate.root_path()) break;
        }
    }
    const std::filesystem::path exe =
        std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) {
        for (std::filesystem::path candidate = exe.parent_path();
             !candidate.empty(); candidate = candidate.parent_path()) {
            if (std::filesystem::is_regular_file(candidate / "VERSION", error) &&
                std::filesystem::is_directory(candidate / "analysis", error)) {
                return candidate;
            }
            if (candidate == candidate.root_path()) break;
        }
    }
    return current.empty() ? std::filesystem::current_path() : current;
}

std::string Fnv1a64File(const std::filesystem::path& path) {
    // Small metadata files only. The reference APK hash is read from the
    // catalogue instead of re-hashing hundreds of megabytes on every launch.
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    const std::string bytes = buffer.str();
    // FNV-1a is not SHA-256; keep the field honest by naming the algorithm.
    std::uint64_t hash = 1469598103934665603ULL;
    for (char raw : bytes) {
        hash ^= static_cast<unsigned char>(raw);
        hash *= 1099511628211ULL;
    }
    std::ostringstream hex;
    hex << std::hex << std::setfill('0') << std::setw(16) << hash;
    return hex.str();
}

bool Exists(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error);
}

void ReadCatalogue(const std::filesystem::path& path,
                   AssetEnvironmentReport& report) {
    report.reference_catalogue_found = Exists(path);
    if (!report.reference_catalogue_found) return;
    std::ifstream stream(path);
    if (!stream) return;
    nlohmann::json document = nlohmann::json::parse(stream, nullptr, false);
    if (document.is_discarded() || !document.is_object() ||
        !document.contains("summary")) {
        return;
    }
    if (document.contains("reference_apk_sha256") &&
        document["reference_apk_sha256"].is_string()) {
        report.reference_apk_sha256 =
            document["reference_apk_sha256"].get<std::string>();
    }
    const auto& summary = document["summary"];
    const auto count = [&](const char* category, const char* key) {
        if (!summary.contains(category) || !summary[category].contains(key)) {
            return 0;
        }
        return summary[category][key].get<int>();
    };
    report.reference_textures = count("textures", "total");
    report.reference_sprites = count("sprites", "total");
    report.reference_weapons = count("weapons", "unique_total");
    report.reference_enemies = count("enemies", "unique_total");
    report.reference_animations = count("animations", "total");
}

} // namespace

AssetEnvironmentReport DetectAssetEnvironment(
    const platform::FileSystem& filesystem) {
    AssetEnvironmentReport report;
    report.project_root = FindProjectRoot();
    if (const char* apk = std::getenv("LITEKNIGHT_REFERENCE_APK");
        apk != nullptr && *apk != '\0') {
        report.reference_apk = apk;
    } else {
        report.reference_apk =
            report.project_root / "original/SoulKnight-2.6.8.apk";
    }
    report.extraction_root = report.project_root / "analysis/apktool_out";
    report.generated_root =
        report.project_root / "assets/generated/visual/r1_1_visual";
    report.runtime_scene =
        filesystem.ResolvePath("scenes/r1_1_visual/scene.json");
    report.inventory_path =
        report.project_root / "analysis/assets/asset_inventory.json";
    report.texture_inventory_path =
        report.project_root / "analysis/assets/texture_inventory.json";
    report.reference_catalogue_path =
        report.project_root / "analysis/assets/reference_catalogue.json";

    report.reference_found = Exists(report.reference_apk) ||
                             Exists(report.extraction_root / "assets/AssetBundles");
    report.generated_found = Exists(report.generated_root / "scene.json");
    report.inventory_found = Exists(report.inventory_path) &&
                             Exists(report.texture_inventory_path);
    if (report.inventory_found) {
        report.asset_inventory_sha256 = Fnv1a64File(report.inventory_path);
    }
    ReadCatalogue(report.reference_catalogue_path, report);

    if (report.inventory_found) {
        std::ifstream stream(report.inventory_path);
        nlohmann::json document = nlohmann::json::parse(stream, nullptr, false);
        if (!document.is_discarded() && document.contains("summary")) {
            if (document.contains("reference_apk_sha256") &&
        document["reference_apk_sha256"].is_string()) {
        report.reference_apk_sha256 =
            document["reference_apk_sha256"].get<std::string>();
    }
    const auto& summary = document["summary"];
            const auto number = [&](const char* key) {
                return summary.contains(key) ? summary[key].get<int>() : 0;
            };
            report.integrated_textures = number("textures_integrated");
            report.integrated_sprites = number("sprites_integrated");
            report.integrated_characters = number("characters_integrated");
            report.integrated_weapons = number("weapons_integrated");
            report.integrated_enemies = number("enemies_integrated");
            if (document.contains("scene_sha256") &&
                document["scene_sha256"].is_string()) {
                report.scene_sha256 = document["scene_sha256"].get<std::string>();
            }
        }
    }
    return report;
}

void LogAssetEnvironment(const AssetEnvironmentReport& report) {
    LOG_INFO_APP("LiteKnight Asset Environment");
    LOG_INFO_APP("  Reference APK:       {} ({})",
                 report.reference_apk.string(),
                 report.reference_found ? "FOUND" : "MISSING");
    LOG_INFO_APP("  Extraction root:     {} ({})",
                 report.extraction_root.string(),
                 Exists(report.extraction_root) ? "FOUND" : "MISSING");
    LOG_INFO_APP("  Generated assets:    {} ({})",
                 report.generated_root.string(),
                 report.generated_found ? "FOUND" : "MISSING");
    LOG_INFO_APP("  Runtime scene:       {} ({})",
                 report.runtime_scene.string(),
                 Exists(report.runtime_scene) ? "FOUND" : "MISSING");
    LOG_INFO_APP("  Asset inventory:     {} ({})",
                 report.inventory_path.string(),
                 report.inventory_found ? "FOUND" : "MISSING");
    LOG_INFO_APP("  Reference catalogue: {} ({})",
                 report.reference_catalogue_path.string(),
                 report.reference_catalogue_found ? "FOUND" : "MISSING");
    LOG_INFO_APP("  Integrated assets:   {} textures / {} sprites / {} chars / {} weapons / {} enemy types",
                 report.integrated_textures, report.integrated_sprites,
                 report.integrated_characters, report.integrated_weapons,
                 report.integrated_enemies);
    LOG_INFO_APP("  Reference available: {} textures / {} sprites / {} weapons / {} enemy IDs / {} clips",
                 report.reference_textures, report.reference_sprites,
                 report.reference_weapons, report.reference_enemies,
                 report.reference_animations);
    LOG_INFO_APP("  Asset root:          {}", report.project_root.string());
}

} // namespace soulknight::app
