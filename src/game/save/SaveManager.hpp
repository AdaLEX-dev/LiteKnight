// Soul Knight Linux — atomic native save slot manager
#pragma once

#include "SaveData.hpp"

#include <filesystem>
#include <string>

namespace soulknight::game::save {

class SaveManager {
public:
    explicit SaveManager(std::filesystem::path directory);

    const std::filesystem::path& Directory() const { return directory_; }
    std::filesystem::path SlotPath() const;
    std::filesystem::path BackupPath() const;
    bool Exists() const;

    // Writes slot_0.json atomically. An existing valid slot is preserved as
    // slot_0.json.bak before replacement. A failed write leaves the previous
    // slot intact; the temporary file is removed.
    bool Save(const SaveData& data, std::string* error);

    // Reads slot_0.json. If it is missing/corrupt/incompatible, the backup is
    // tried. A future save version is never overwritten automatically.
    bool Load(SaveData* data, std::string* error);

    bool Delete(std::string* error);

private:
    std::filesystem::path directory_;
};

} // namespace soulknight::game::save
