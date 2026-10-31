// Soul Knight Linux — File System Implementation
#include "FileSystem.hpp"
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <engine/core/Log.hpp>

namespace soulknight::platform {

bool FileSystem::Initialize() {
    // XDG Base Directory Specification
    // $XDG_DATA_HOME  -> ~/.local/share
    // $XDG_CONFIG_HOME -> ~/.config
    // $XDG_CACHE_HOME -> ~/.cache

    const char* xdg_data = std::getenv("XDG_DATA_HOME");
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    const char* xdg_cache = std::getenv("XDG_CACHE_HOME");

    const char* home = std::getenv("HOME");

    std::filesystem::path base_home = home ? home : ".";

    data_home_ = xdg_data ? xdg_data : (base_home / ".local" / "share");
    config_home_ = xdg_config ? xdg_config : (base_home / ".config");
    cache_home_ = xdg_cache ? xdg_cache : (base_home / ".cache");

    // App-specific directories
    saves_path_ = data_home_ / "soul-knight" / "saves";
    assets_path_ = data_home_ / "soul-knight" / "assets";
    logs_path_ = cache_home_ / "soul-knight" / "logs";
    config_file_ = config_home_ / "soul-knight" / "config.toml";

    // Create directories
    std::error_code ec;
    std::filesystem::create_directories(saves_path_, ec);
    std::filesystem::create_directories(assets_path_, ec);
    std::filesystem::create_directories(logs_path_, ec);
    std::filesystem::create_directories(config_file_.parent_path(), ec);

    // Executable directory (for built-in assets)
    exe_dir_ = std::filesystem::current_path(); // TODO: get actual exe path

    // Search paths (priority order)
    search_paths_ = {
        assets_path_,           // User overrides
        exe_dir_ / "assets",    // Development build
        std::filesystem::path("/usr/share/soul-knight/assets")  // System install
    };

    LOG_INFO_ASSET("FileSystem initialized");
    LOG_INFO_ASSET("  Data:   {}", data_home_.string());
    LOG_INFO_ASSET("  Config: {}", config_home_.string());
    LOG_INFO_ASSET("  Cache:  {}", cache_home_.string());
    LOG_INFO_ASSET("  Saves:  {}", saves_path_.string());
    LOG_INFO_ASSET("  Assets: {}", assets_path_.string());
    LOG_INFO_ASSET("  Logs:   {}", logs_path_.string());

    return true;
}

std::optional<std::vector<uint8_t>> FileSystem::ReadFile(const std::filesystem::path& relative_path) const {
    auto full_path = ResolvePath(relative_path);
    if (full_path.empty()) return {};

    std::ifstream file(full_path, std::ios::binary | std::ios::ate);
    if (!file) return {};

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (size > 0) {
        file.read(reinterpret_cast<char*>(buffer.data()), size);
    }
    return buffer;
}

std::optional<std::string> FileSystem::ReadTextFile(const std::filesystem::path& relative_path) const {
    auto full_path = ResolvePath(relative_path);
    if (full_path.empty()) return {};

    std::ifstream file(full_path);
    if (!file) return {};

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool FileSystem::WriteFile(const std::filesystem::path& relative_path, const std::vector<uint8_t>& data) const {
    std::filesystem::path full_path = assets_path_ / relative_path;

    std::error_code ec;
    std::filesystem::create_directories(full_path.parent_path(), ec);
    if (ec) {
        LOG_ERROR_ASSET("Failed to create directories for {}: {}", full_path.string(), ec.message());
        return false;
    }

    std::ofstream file(full_path, std::ios::binary);
    if (!file) {
        LOG_ERROR_ASSET("Failed to open {} for writing", full_path.string());
        return false;
    }

    if (!data.empty()) {
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
    return file.good();
}

bool FileSystem::WriteFile(const std::filesystem::path& relative_path, const std::string& text) const {
    std::filesystem::path full_path = assets_path_ / relative_path;

    std::error_code ec;
    std::filesystem::create_directories(full_path.parent_path(), ec);
    if (ec) return false;

    std::ofstream file(full_path);
    if (!file) return false;

    file << text;
    return file.good();
}

bool FileSystem::DeleteFile(const std::filesystem::path& relative_path) const {
    std::filesystem::path full_path = assets_path_ / relative_path;
    std::error_code ec;
    std::filesystem::remove(full_path, ec);
    return !ec;
}

bool FileSystem::Exists(const std::filesystem::path& relative_path) const {
    return std::filesystem::exists(ResolvePath(relative_path));
}

std::vector<std::filesystem::path> FileSystem::ListDirectory(const std::filesystem::path& relative_path) const {
    std::vector<std::filesystem::path> result;
    auto full_path = ResolvePath(relative_path);
    if (full_path.empty()) return result;

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(full_path, ec)) {
        if (!ec) {
            result.push_back(entry.path().filename());
        }
    }
    return result;
}

int64_t FileSystem::GetModifiedTime(const std::filesystem::path& relative_path) const {
    auto full_path = ResolvePath(relative_path);
    if (full_path.empty()) return -1;

    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(full_path, ec);
    if (ec) return -1;

    auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
}

int64_t FileSystem::GetFileSize(const std::filesystem::path& relative_path) const {
    auto full_path = ResolvePath(relative_path);
    if (full_path.empty()) return -1;

    std::error_code ec;
    auto size = std::filesystem::file_size(full_path, ec);
    return ec ? -1 : static_cast<int64_t>(size);
}

std::filesystem::path FileSystem::ResolvePath(const std::filesystem::path& relative_path) const {
    // Try search paths in order
    for (const auto& base : search_paths_) {
        auto full = base / relative_path;
        if (std::filesystem::exists(full)) {
            return full;
        }
    }
    return {};
}

} // namespace soulknight::platform