#include "SaveManager.hpp"

#include "SaveSerializer.hpp"

#include <fstream>
#include <system_error>

namespace soulknight::game::save {
namespace {

bool ParseFile(const std::filesystem::path& path, Json* document,
               std::string* error) {
    std::ifstream stream(path);
    if (!stream) {
        if (error != nullptr) *error = "cannot open save: " + path.string();
        return false;
    }
    Json parsed = Json::parse(stream, nullptr, false);
    if (parsed.is_discarded()) {
        if (error != nullptr) *error = "save JSON is corrupt: " + path.string();
        return false;
    }
    *document = std::move(parsed);
    return true;
}

bool WriteAtomic(const std::filesystem::path& path, const Json& document,
                 std::string* error) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        if (error != nullptr) *error = "cannot create save directory: " + ec.message();
        return false;
    }
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            if (error != nullptr) *error = "cannot open temporary save: " +
                                            temporary.string();
            return false;
        }
        stream << document.dump(2) << '\n';
        stream.flush();
        if (!stream.good()) {
            stream.close();
            std::filesystem::remove(temporary, ec);
            if (error != nullptr) *error = "failed writing temporary save";
            return false;
        }
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        // Never delete the previous slot as a fallback: a failed rename leaves
        // the old save and backup intact for recovery.
        std::filesystem::remove(temporary, ec);
        if (error != nullptr) *error = "cannot commit save: " + ec.message();
        return false;
    }
    return true;
}

} // namespace

SaveManager::SaveManager(std::filesystem::path directory)
    : directory_(std::move(directory)) {}

std::filesystem::path SaveManager::SlotPath() const {
    return directory_ / "slot_0.json";
}

std::filesystem::path SaveManager::BackupPath() const {
    return directory_ / "slot_0.json.bak";
}

bool SaveManager::Exists() const {
    std::error_code ec;
    return std::filesystem::is_regular_file(SlotPath(), ec);
}

bool SaveManager::Save(const SaveData& data, std::string* error) {
    if (error != nullptr) error->clear();
    SaveData copy = data;
    copy.save_version = SaveData::kCurrentVersion;
    const Json document = Serialize(copy);

    std::error_code ec;
    std::filesystem::create_directories(directory_, ec);
    if (ec) {
        if (error != nullptr) *error = "cannot create save directory: " + ec.message();
        return false;
    }

    // Refresh backup from the previous slot only if it parses and is supported.
    if (std::filesystem::is_regular_file(SlotPath(), ec)) {
        Json previous;
        std::string ignored;
        if (ParseFile(SlotPath(), &previous, &ignored) &&
            MigrateToCurrent(previous, &ignored) &&
            previous.value("save_version", 0) == SaveData::kCurrentVersion) {
            std::filesystem::copy_file(SlotPath(), BackupPath(),
                                       std::filesystem::copy_options::overwrite_existing,
                                       ec);
        }
    }

    std::string write_error;
    if (!WriteAtomic(SlotPath(), document, &write_error)) {
        if (error != nullptr) *error = std::move(write_error);
        return false;
    }
    return true;
}

bool SaveManager::Load(SaveData* data, std::string* error) {
    if (data == nullptr) return false;
    if (error != nullptr) error->clear();
    std::string primary_error;
    const auto try_path = [&](const std::filesystem::path& path,
                              std::string* path_error) -> bool {
        Json document;
        if (!ParseFile(path, &document, path_error)) return false;
        if (!MigrateToCurrent(document, path_error)) return false;
        SaveData loaded;
        if (!Deserialize(document, &loaded, path_error)) return false;
        *data = std::move(loaded);
        return true;
    };

    if (try_path(SlotPath(), &primary_error)) return true;
    std::string backup_error;
    if (try_path(BackupPath(), &backup_error)) {
        if (error != nullptr) {
            *error = "primary save failed; loaded backup (" + primary_error + ")";
        }
        return true;
    }
    if (error != nullptr) {
        *error = "cannot load save: primary=" + primary_error +
                 "; backup=" + backup_error;
    }
    return false;
}

bool SaveManager::Delete(std::string* error) {
    if (error != nullptr) error->clear();
    std::error_code ec;
    std::filesystem::remove(SlotPath(), ec);
    if (ec) {
        if (error != nullptr) *error = "cannot delete save: " + ec.message();
        return false;
    }
    std::filesystem::remove(BackupPath(), ec);
    return true;
}

} // namespace soulknight::game::save
