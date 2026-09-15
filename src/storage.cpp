#include "storage.hpp"

#include <fstream>
#include <stdexcept>

Storage::Storage(const std::string& path) : path_(path) {}

void Storage::appendPut(const std::string& key, const std::string& value) {
    std::ofstream file(path_, std::ios::binary | std::ios::app);
    if (!file) {
        throw std::runtime_error("Failed to open database file");
    }

    const std::uint8_t type = static_cast<std::uint8_t>(RecordType::Put);

    const std::uint32_t keyLength = static_cast<std::uint32_t>(key.size());

    const std::uint32_t valueLength = static_cast<std::uint32_t>(value.size());

    file.write(reinterpret_cast<const char*>(&type), sizeof(type));

    file.write(reinterpret_cast<const char*>(&keyLength), sizeof(keyLength));

    file.write(reinterpret_cast<const char*>(&valueLength),
               sizeof(valueLength));

    file.write(key.data(), keyLength);
    file.write(value.data(), valueLength);

    if (!file) {
        throw std::runtime_error("Failed to write database record");
    }
}

void Storage::appendDelete(const std::string& key) {
    std::ofstream file(path_, std::ios::binary | std::ios::app);
    if (!file) {
        throw std::runtime_error("Failed to open database file");
    }

    const std::uint8_t type = static_cast<std::uint8_t>(RecordType::Delete);

    const std::uint32_t keyLength = static_cast<std::uint32_t>(key.size());

    file.write(reinterpret_cast<const char*>(&type), sizeof(type));

    file.write(reinterpret_cast<const char*>(&keyLength), sizeof(keyLength));

    file.write(key.data(), keyLength);

    if (!file) {
        throw std::runtime_error("Failed to write database record");
    }
}

std::vector<Record> Storage::readAll() const {
    std::vector<Record> records;

    std::ifstream file(path_, std::ios::binary);
    if (!file) {
        // For now, a missing database file means an empty database.
        // TODO: make an actual handler for this
        return records;
    }

    while (true) {
        std::uint8_t rawType;
        file.read(reinterpret_cast<char*>(&rawType), sizeof(rawType));

        // If we couldn't read the next record type because we reached
        // the end of the file, that's a normal end of the log.
        if (file.eof()) {
            break;
        }

        if (!file) {
            throw std::runtime_error("Failed while reading database file");
        }

        if (rawType != static_cast<std::uint8_t>(RecordType::Put) &&
            rawType != static_cast<std::uint8_t>(RecordType::Delete)) {
            throw std::runtime_error("Corrupted database: unknown record type");
        }

        std::uint32_t keyLength;
        file.read(reinterpret_cast<char*>(&keyLength), sizeof(keyLength));
        if (!file) {
            throw std::runtime_error(
                "Corrupted database: incomplete key length");
        }

        std::uint32_t valueLength = 0;
        if (rawType == static_cast<std::uint8_t>(RecordType::Put)) {
            file.read(reinterpret_cast<char*>(&valueLength),
                      sizeof(valueLength));

            if (!file) {
                throw std::runtime_error(
                    "Corrupted database: incomplete value length");
            }
        }

        std::string key(keyLength, '\0');
        file.read(key.data(), keyLength);
        if (!file) {
            throw std::runtime_error("Corrupted database: incomplete key");
        }

        std::string value;
        if (rawType == static_cast<std::uint8_t>(RecordType::Put)) {
            value.resize(valueLength);

            file.read(value.data(), valueLength);

            if (!file) {
                throw std::runtime_error(
                    "Corrupted database: incomplete value");
            }
        }

        records.push_back({static_cast<RecordType>(rawType), key, value});
    }

    return records;
}