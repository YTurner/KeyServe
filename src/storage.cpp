#include "storage.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

#include "errors.hpp"

// CRC-32/IEEE parameters used for record integrity checks.
static constexpr std::uint32_t CRC_POLYNOMIAL = 0xEDB88320;
static constexpr std::uint32_t CRC_INITIAL_VALUE = 0xFFFFFFFF;
static constexpr std::uint32_t CRC_FINAL_XOR_VALUE = 0xFFFFFFFF;

namespace {

// Appends a uint32_t as four little-endian bytes.
// The file format uses a fixed byte order instead of the host machine's byte order.
void appendUint32LE(std::vector<char>& buffer, std::uint32_t value) {
    buffer.push_back(static_cast<char>(value & 0xFF));
    buffer.push_back(static_cast<char>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<char>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<char>((value >> 24) & 0xFF));
}

} // namespace

Storage::Storage(const std::string& path) : path_(path) {
    if (!std::filesystem::exists(path_)) {
        initializeFile();
    } else {
        validateFile();
    }
}

// Creates a new database file containing only the file signature and format version.
void Storage::initializeFile() {
    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw StorageError("Failed to create database file");
    }

    file.write(FILE_MAGIC, sizeof(FILE_MAGIC)); // file signature

    const std::uint8_t version = FILE_VERSION;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    if (!file) {
        throw StorageError("Failed to initialize database file");
    }
}

// Validates the database signature and file-format version before records are read.
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

    file.write(reinterpret_cast<const char*>(&valueLength), sizeof(valueLength));

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
            throw std::runtime_error("Corrupted database: incomplete key length");
        }

        std::uint32_t valueLength = 0;
        if (rawType == static_cast<std::uint8_t>(RecordType::Put)) {
            file.read(reinterpret_cast<char*>(&valueLength), sizeof(valueLength));

            if (!file) {
                throw std::runtime_error("Corrupted database: incomplete value length");
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
                throw std::runtime_error("Corrupted database: incomplete value");
            }
        }

        records.push_back({static_cast<RecordType>(rawType), key, value});
    }

    return records;
}

// Serializes only the type-specific payload of a record.
// PUT:    [keyLength][key][valueLength][value]
// DELETE: [keyLength][key]
// Input validation is handled by serializeRecord().
std::vector<char> Storage::serializePayload(const Record& record) const {

    std::vector<char> payload;

    const auto keyLength = static_cast<std::uint32_t>(record.key.size());

    appendUint32LE(payload, keyLength);

    payload.insert(payload.end(), record.key.begin(), record.key.end());

    switch (record.type) {
    case RecordType::Put: {
        const auto valueLength = static_cast<std::uint32_t>(record.value.size());

        appendUint32LE(payload, valueLength);

        payload.insert(payload.end(), record.value.begin(), record.value.end());

        break;
    }

    case RecordType::Delete:
        break;

    default:
        throw std::logic_error("Invalid record type during serialization");
    }

    return payload;
}

// Serializes a complete v2 record in memory:
// [type][payloadLength][checksum][payload]
//
// The CRC covers type + payloadLength + payload.
// The checksum field itself is not included in the CRC.
std::vector<char> Storage::serializeRecord(const Record& record) const {
    if (record.key.size() > MAX_KEY_SIZE) {
        throw StorageError("Key exceeds maximum allowed size");
    }

    if (record.value.size() > MAX_VALUE_SIZE) {
        throw StorageError("Value exceeds maximum allowed size");
    }

    if (record.type == RecordType::Delete && !record.value.empty()) {
        throw std::logic_error("Invalid record type and value combination during serialization");
    }

    if (record.type != RecordType::Put && record.type != RecordType::Delete) {
        throw std::logic_error("Invalid record type during serialization");
    }

    std::vector<char> recordData;

    recordData.push_back(static_cast<char>(record.type));

    const auto payload = serializePayload(record);

    // Protect against future format changes producing a payload that cannot fit
    // in the uint32_t payloadLength field.
    if (payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw StorageError("Payload exceeds maximum allowed size");
    }
    appendUint32LE(recordData, static_cast<std::uint32_t>(payload.size()));

    // Build the exact byte sequence covered by the CRC, excluding the checksum field.
    std::vector<char> checkSumBuffer;
    checkSumBuffer.reserve(recordData.size() + payload.size());
    checkSumBuffer.insert(checkSumBuffer.end(), recordData.begin(), recordData.end());
    checkSumBuffer.insert(checkSumBuffer.end(), payload.begin(), payload.end());

    appendUint32LE(recordData, calculateChecksum(checkSumBuffer));
    recordData.insert(recordData.end(), payload.begin(), payload.end());

    return recordData;
}

// Calculates CRC-32/IEEE over the supplied serialized bytes.
std::uint32_t Storage::calculateChecksum(const std::vector<char>& data) const {
    std::uint32_t crc = CRC_INITIAL_VALUE;
    for (const auto& byte : data) {
        crc ^= static_cast<std::uint8_t>(byte);
        for (int i = 0; i < 8; ++i) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CRC_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ CRC_FINAL_XOR_VALUE;
}