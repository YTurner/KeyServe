#include "storage.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "errors.hpp"

Storage::Storage(const std::string& path) : path_(path) {
    if (!std::filesystem::exists(path_)) {
        initilizeFile();
    } else {
        validateFile();
    }
}

void Storage::initilizeFile() {
    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw StorageError("Failed to create database file");
    }

    file.write(FILE_MAGIC, sizeof(FILE_MAGIC));  // file signature

    const std::uint8_t version = FILE_VERSION;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    if (!file) {
        throw StorageError("Failed to initialize database file");
    }
}

void Storage::validateFile() const {
    std::ifstream file(path_, std::ios::binary);
    if (!file) {
        throw StorageError("Failed to open database file");
    }

    char magic_header[4];
    file.read(magic_header, sizeof(magic_header));
    if (!file) {
        throw CorruptionError("Incomplete database header");
    }
    if (std::memcmp(FILE_MAGIC, magic_header, sizeof(FILE_MAGIC)) != 0) {
        throw CorruptionError("Invalid KeyServe file header");
    }

    std::uint8_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!file) {
        throw CorruptionError("Incomplete database header");
    }
    if (version != FILE_VERSION) {
        throw CorruptionError("Unsupported KeyServe database version");
    }
}

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
        throw StorageError("Failed to open database file");
    }

    file.seekg(sizeof(FILE_MAGIC) + sizeof(FILE_VERSION), std::ios::beg);
    if (!file) {
        throw StorageError("Failed to seek past database header");
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