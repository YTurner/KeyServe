#include "database.hpp"

Database::Database(const std::string& path) : storage_(path) {
    auto records = storage_.readAll();

    /// Rebuild the current in-memory state by replaying the persistent operation log.
    //* Replay modifies data_ directly so recovered operations are not appended again.
    for (const auto& record : records) {
        if (record.type == RecordType::Put) {
            data_.insert_or_assign(record.key, record.value);
        } else if (record.type == RecordType::Delete) {
            data_.erase(record.key);
        }
    }
}

void Database::put(const std::string& key, const std::string& value) {
    //* Persist first so RAM is only updated after the disk operation succeeds.
    storage_.appendPut(key, value);
    data_.insert_or_assign(key, value);
}

std::optional<std::string> Database::get(const std::string& key) const {
    auto it = data_.find(key);

    if (it == data_.end()) {
        return std::nullopt;
    }

    return it->second;
}

bool Database::remove(const std::string& key) {
    auto it = data_.find(key);

    if (it == data_.end()) {
        return false;
    }

    //* Persist first so RAM is only updated after the disk operation succeeds.
    storage_.appendDelete(key);
    data_.erase(it);

    return true;
}

bool Database::exists(const std::string& key) const {
    return data_.find(key) != data_.end();
}