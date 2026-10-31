// Soul Knight Linux — File System (XDG Compliant)
#pragma once

#include <string>
#include <filesystem>
#include <optional>
#include <vector>
#include <engine/core/Types.hpp>

namespace soulknight::platform {

class FileSystem {
public:
    FileSystem() = default;
    ~FileSystem() = default;

    bool Initialize();

    // Paths
    const std::filesystem::path& GetDataHome() const { return data_home_; }
    const std::filesystem::path& GetConfigHome() const { return config_home_; }
    const std::filesystem::path& GetCacheHome() const { return cache_home_; }

    const std::filesystem::path& GetSavesPath() const { return saves_path_; }
    const std::filesystem::path& GetAssetsPath() const { return assets_path_; }
    const std::filesystem::path& GetLogsPath() const { return logs_path_; }
    const std::filesystem::path& GetConfigFilePath() const { return config_file_; }

    // File operations
    std::optional<std::vector<uint8_t>> ReadFile(const std::filesystem::path& relative_path) const;
    std::optional<std::string> ReadTextFile(const std::filesystem::path& relative_path) const;
    bool WriteFile(const std::filesystem::path& relative_path, const std::vector<uint8_t>& data) const;
    bool WriteFile(const std::filesystem::path& relative_path, const std::string& text) const;
    bool DeleteFile(const std::filesystem::path& relative_path) const;
    bool Exists(const std::filesystem::path& relative_path) const;
    std::vector<std::filesystem::path> ListDirectory(const std::filesystem::path& relative_path) const;
    int64_t GetModifiedTime(const std::filesystem::path& relative_path) const;
    int64_t GetFileSize(const std::filesystem::path& relative_path) const;

    // Search paths (priority order)
    std::filesystem::path ResolvePath(const std::filesystem::path& relative_path) const;

private:
    std::filesystem::path data_home_;
    std::filesystem::path config_home_;
    std::filesystem::path cache_home_;

    std::filesystem::path saves_path_;
    std::filesystem::path assets_path_;
    std::filesystem::path logs_path_;
    std::filesystem::path config_file_;

    std::filesystem::path exe_dir_;
    std::vector<std::filesystem::path> search_paths_;
};

} // namespace soulknight::platform