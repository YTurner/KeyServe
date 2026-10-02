#include "database.hpp"

Database::Database(const std::string& path) : storage_(path) {
    // Rebuild the current in-memory state by replaying the persistent operation log.
    //* Replay modifies data_ directly so recovered operations are not appended again.
    storage_.replay([this](const Record& record) {
        if (record.type == RecordType::Put) {
            data_.insert_or_assign(record.key, record.value);
        } else if (record.type == RecordType::Delete) {
            data_.erase(record.key);
        }
    });

    storage_.initializeSizeAccounting([&](const EntrySink& sink) {
        for (const auto& [key, value] : data_) {
            sink(key, value);
        }
    });
}

void Database::put(const std::string& key, const std::string& value) {
    //* Persist first so RAM is only updated after the disk operation succeeds.
    auto it = data_.find(key);
    const std::string* oldValue = (it != data_.end()) ? &it->second : nullptr;
    storage_.appendPut(key, value, oldValue);
    data_.insert_or_assign(key, value);
    if (storage_.shouldCompact()) {
        compact();
    }
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
    storage_.appendDelete(key, it->second);
    data_.erase(it);

    if (storage_.shouldCompact()) {
        compact();
    }

    return true;
}

bool Database::exists(const std::string& key) const {
    return data_.find(key) != data_.end();
}

bool Database::compact() {
    return storage_.compact([this](const EntrySink& sink) {
        for (const auto& [key, value] : data_) {
            sink(key, value);
        }
    });
}