#include "storage.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

#include "errors.hpp"

// Binary record encoding/decoding helpers used only by this translation unit.
namespace {

// Key Serve DataBase magic bytes for db file signature
constexpr char FILE_MAGIC[4] = {'K', 'S', 'D', 'B'};
constexpr std::uint8_t FILE_VERSION = 2;

constexpr std::uint32_t MAX_KEY_SIZE = 1024 * 1024;        // 1 MiB
constexpr std::uint32_t MAX_VALUE_SIZE = 64 * 1024 * 1024; // 64 MiB

// CRC-32/IEEE parameters used for record integrity checks.
static constexpr std::uint32_t CRC_POLYNOMIAL = 0xEDB88320;
static constexpr std::uint32_t CRC_INITIAL_VALUE = 0xFFFFFFFF;
static constexpr std::uint32_t CRC_FINAL_XOR_VALUE = 0xFFFFFFFF;

// Appends a uint32_t as four little-endian bytes.
// The file format uses a fixed byte order instead of the host machine's byte order.
void appendUint32LE(std::vector<char>& buffer, std::uint32_t value) {
    buffer.push_back(static_cast<char>(value & 0xFF));
    buffer.push_back(static_cast<char>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<char>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<char>((value >> 24) & 0xFF));
}

// Reads four little-endian bytes as a uint32_t.
// On success, advances offset by four bytes.
// On failure, returns nullopt and leaves offset unchanged.
std::optional<std::uint32_t> tryReadUint32LE(const std::vector<char>& buffer, size_t& offset) {
    if (offset > buffer.size() || buffer.size() - offset < 4) {
        return std::nullopt;
    }

    std::uint32_t result =
        static_cast<std::uint32_t>(static_cast<std::uint8_t>(buffer[offset])) |
        (static_cast<std::uint32_t>(static_cast<std::uint8_t>(buffer[offset + 1])) << 8) |
        (static_cast<std::uint32_t>(static_cast<std::uint8_t>(buffer[offset + 2])) << 16) |
        (static_cast<std::uint32_t>(static_cast<std::uint8_t>(buffer[offset + 3])) << 24);

    offset += 4;
    return result;
}

// Reads count bytes from buffer starting from offset and increments offset by count
std::optional<std::vector<char>> tryReadBytes(const std::vector<char>& buffer, size_t& offset,
                                              size_t count) {
    if (offset > buffer.size() || count > buffer.size() - offset) {
        return std::nullopt;
    }

    std::vector<char> data(buffer.begin() + offset, buffer.begin() + offset + count);
    offset += count;
    return data;
}

// reads as many bytes as it can to buffer, retrying partial reads and EINTR.
// returns how many bytes it read
//* POSIX read() may succeed without reading all required bytes.
size_t readUpTo(int fd, char* ptr, size_t numberOfBytes) {
    size_t bytesLeft = numberOfBytes;
    while (bytesLeft > 0) {
        ssize_t bytesRead = read(fd, ptr, bytesLeft);

        if (bytesRead == 0) {
            return (numberOfBytes - bytesLeft);
        }

        if (bytesRead < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw StorageError("Failed to read from database file");
        }

        ptr += bytesRead;
        bytesLeft -= bytesRead;
    }

    return (numberOfBytes - bytesLeft);
}
// Calculates CRC-32/IEEE over the supplied serialized bytes.
// TODO: Replace bit-at-a-time CRC32 with a table-based implementation.
std::uint32_t calculateChecksum(const std::vector<char>& data) {
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

// Serializes only the type-specific payload of a record.
// PUT:    [keyLength][key][valueLength][value]
// DELETE: [keyLength][key]
// Input validation is handled by serializeRecord().
std::vector<char> serializePayload(const Record& record) {

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
std::vector<char> serializeRecord(const Record& record) {
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

Record parsePayload(RecordType type, const std::vector<char>& payload) {
    size_t offset = 0;

    std::optional<uint32_t> keyLength = tryReadUint32LE(payload, offset);

    if (!keyLength.has_value()) {
        throw CorruptionError("Invalid key length size");
    }
    if (*keyLength > MAX_KEY_SIZE) {
        throw CorruptionError("Key exceeds maximum allowed size");
    }

    std::optional<std::vector<char>> key = tryReadBytes(payload, offset, *keyLength);
    if (!key.has_value()) {
        throw CorruptionError("Invalid key");
    }

    switch (type) {
    case RecordType::Delete:
        if (offset != payload.size()) {
            throw CorruptionError("Garbage values after payload");
        }
        return {RecordType::Delete, std::string(key->begin(), key->end()), ""};
    case RecordType::Put:
        break;
    default:
        throw std::logic_error("Invalid record type");
    }

    std::optional<uint32_t> valueLength = tryReadUint32LE(payload, offset);

    if (!valueLength.has_value()) {
        throw CorruptionError("Invalid value length size");
    }
    if (*valueLength > MAX_VALUE_SIZE) {
        throw CorruptionError("Value exceeds maximum allowed size");
    }

    std::optional<std::vector<char>> value = tryReadBytes(payload, offset, *valueLength);
    if (!value.has_value()) {
        throw CorruptionError("Invalid value");
    }

    if (offset != payload.size()) {
        throw CorruptionError("Garbage values after payload");
    }

    return {RecordType::Put, std::string(key->begin(), key->end()),
            std::string(value->begin(), value->end())};
}

// Reads and validates one v2 record from the current file-descriptor offset.
//* Clean EOF, incomplete tail, corruption, and I/O failure have different meanings.
std::optional<Record> readRecord(int fd) {
    char typeByte;
    if (readUpTo(fd, &typeByte, 1) == 0) {
        return std::nullopt;
    }
    RecordType type;
    switch (static_cast<RecordType>(typeByte)) {
    case RecordType::Put:
        type = RecordType::Put;
        break;
    case RecordType::Delete:
        type = RecordType::Delete;
        break;
    default:
        throw CorruptionError("Invalid type in record");
    }

    // Read the fixed 8-byte record header fields: payload length and checksum.
    std::vector<char> buffer(2 * 4);
    if (readUpTo(fd, buffer.data(), buffer.size()) < 2 * 4) {
        throw IncompleteRecordError("Incomplete database record");
    }

    size_t bufferOffset = 0;
    // The fixed header read above guarantees both uint32 fields are present.
    uint32_t payloadLength = *tryReadUint32LE(buffer, bufferOffset);
    uint32_t maxPayloadSize;
    if (type == RecordType::Delete) {
        maxPayloadSize = MAX_KEY_SIZE + 4;
    } else {
        maxPayloadSize = MAX_KEY_SIZE + MAX_VALUE_SIZE + 2 * 4;
    }
    if (payloadLength > maxPayloadSize) {
        throw CorruptionError("Payload exceeds maximum allowed size");
    }

    uint32_t checkSum = *tryReadUint32LE(buffer, bufferOffset);

    std::vector<char> payload(payloadLength);
    if (readUpTo(fd, payload.data(), payload.size()) < payload.size()) {
        throw IncompleteRecordError("Incomplete database record");
    }

    std::vector<char> checkSumBuffer;
    checkSumBuffer.reserve(sizeof(type) + sizeof(payloadLength) + payload.size());
    checkSumBuffer.push_back(static_cast<char>(type));
    appendUint32LE(checkSumBuffer, static_cast<std::uint32_t>(payload.size()));
    checkSumBuffer.insert(checkSumBuffer.end(), payload.begin(), payload.end());
    uint32_t checkSumTest = calculateChecksum(checkSumBuffer);
    if (checkSum != checkSumTest) {
        throw CorruptionError("Invalid checksum");
    }

    return parsePayload(type, payload);
}

// Writes the entire buffer, retrying partial writes and EINTR.
//* POSIX write() may succeed without consuming the entire buffer.
void writeAll(int fd, const char* ptr, size_t numberOfBytes) {
    size_t bytesLeft = numberOfBytes;
    while (bytesLeft > 0) {
        ssize_t bytesWritten = write(fd, ptr, bytesLeft);

        if (bytesWritten == 0) {
            throw StorageError("Failed to write to database file");
        }

        if (bytesWritten < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw StorageError("Failed to write  to database file");
        }

        ptr += bytesWritten;
        bytesLeft -= bytesWritten;
    }
}

// Opens the persistent append-only write descriptor used for database updates.
int openWriteDescriptor(const std::string& path) {
    int fd = open(path.c_str(), O_WRONLY | O_APPEND | O_CLOEXEC);
    if (fd < 0) {
        throw StorageError("Failed to open database file");
    }
    return fd;
}

// Opens the persistent read descriptor used for database checks.
int openReadDescriptor(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        throw StorageError("Failed to open database file");
    }
    return fd;
}

void writeDatabaseHeader(int fd) {
    writeAll(fd, FILE_MAGIC, sizeof(FILE_MAGIC)); // file signature

    const std::uint8_t version = FILE_VERSION;
    writeAll(fd, reinterpret_cast<const char*>(&version), sizeof(version));
}

} // namespace

Storage::Storage(const std::string& path) : path_(path), writeFd_(), readFd_() {
    if (!std::filesystem::exists(path_)) {
        writeFd_.reset(initializeFile());
        readFd_.reset(openReadDescriptor(path_));
    } else {
        readFd_.reset(validateFile());
        writeFd_.reset(openWriteDescriptor(path_));
    }
}

// Creates a new database file containing only the file signature and format version.
int Storage::initializeFile() {
    int fd = open(path_.c_str(), O_CREAT | O_WRONLY | O_APPEND | O_EXCL | O_CLOEXEC, 0666);
    if (fd < 0) {
        throw StorageError("Failed to create database file");
    }
    UniqueFd tempUniqueFd(fd);
    writeDatabaseHeader(tempUniqueFd.get());
    if (fdatasync(tempUniqueFd.get()) < 0) {
        throw StorageError("Failed to sync database file");
    }
    return (tempUniqueFd.release());
}

// Validates the database signature and file-format version before records are read.
int Storage::validateFile() const {
    UniqueFd tempUniqeFd(openReadDescriptor(path_));

    char magic_header[4];
    size_t magicReadCheck = readUpTo(tempUniqeFd.get(), magic_header, sizeof(magic_header));
    if (magicReadCheck < sizeof(magic_header)) {
        throw CorruptionError("Incomplete KeyServe file header");
    }
    if (std::memcmp(FILE_MAGIC, magic_header, sizeof(FILE_MAGIC)) != 0) {
        throw CorruptionError("Invalid KeyServe file header");
    }

    std::uint8_t version;
    size_t versionReadCheck =
        readUpTo(tempUniqeFd.get(), reinterpret_cast<char*>(&version), sizeof(version));
    if (versionReadCheck < sizeof(version)) {
        throw CorruptionError("Incomplete KeyServe file header");
    }
    if (version != FILE_VERSION) {
        throw CorruptionError("Unsupported KeyServe database version");
    }

    return (tempUniqeFd.release());
}

void Storage::appendPut(const std::string& key, const std::string& value) {
    std::vector<char> serializedRecord = serializeRecord(Record{RecordType::Put, key, value});
    writeAll(writeFd_.get(), serializedRecord.data(), serializedRecord.size());
    if (fdatasync(writeFd_.get()) < 0) {
        throw StorageError("Failed to sync database file");
    }
}

void Storage::appendDelete(const std::string& key) {
    std::vector<char> serializedRecord = serializeRecord(Record{RecordType::Delete, key, ""});
    writeAll(writeFd_.get(), serializedRecord.data(), serializedRecord.size());
    if (fdatasync(writeFd_.get()) < 0) {
        throw StorageError("Failed to sync database file");
    }
}

// Replays valid records in order.
//* Only an incomplete final record is auto-recovered; other corruption stays fatal.
void Storage::replay(const std::function<void(const Record&)>& applyRecord) {
    if (lseek(readFd_.get(), sizeof(FILE_MAGIC) + sizeof(FILE_VERSION), SEEK_SET) == -1) {
        throw StorageError("Failed to seek past database header");
    }
    while (true) {
        std::optional<Record> record;
        off_t lastRead = lseek(readFd_.get(), 0, SEEK_CUR);
        if (lastRead == -1) {
            throw StorageError("Failed to read database file");
        }
        try {
            record = readRecord(readFd_.get());
        } catch (const IncompleteRecordError&) {
            recoverIncompleteTail(lastRead);
            return;
        }
        if (!record.has_value()) {
            break;
        }
        applyRecord(*record);
    }
}

// Removes an incomplete final record by truncating to the last valid boundary.
//* The repaired file is synced before recovery is considered successful.
void Storage::recoverIncompleteTail(off_t lastValidLength) {
    if (ftruncate(writeFd_.get(), lastValidLength) < 0) {
        throw StorageError("Failed to truncate database file");
    }
    if (fdatasync(writeFd_.get()) < 0) {
        throw StorageError("Failed to sync database file");
    }
}

//! The active database must remain untouched until the complete temp database*
//! has been written and synced successfully.
bool Storage::compact(const EntrySource& source) {
    bool renamed = false;
    std::string tempPath = path_ + ".compact.XXXXXX";
    try {
        int tempFd = mkostemp(tempPath.data(), O_APPEND | O_CLOEXEC);
        if (tempFd < 0) {
            return false;
        }
        UniqueFd uniqueTempFd(tempFd);
        struct stat dbStats;
        if (fstat(writeFd_.get(), &dbStats) == -1) {
            unlink(tempPath.c_str());
            return false;
        }
        mode_t dbPerms =
            dbStats.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX);

        if (fchmod(uniqueTempFd.get(), dbPerms) == -1) {
            unlink(tempPath.c_str());
            return false;
        }

        writeDatabaseHeader(uniqueTempFd.get());
        EntrySink sink = [&](const std::string& key, const std::string& value) {
            std::vector<char> serializedRecord = serializeRecord({RecordType::Put, key, value});
            writeAll(uniqueTempFd.get(), serializedRecord.data(), serializedRecord.size());
        };
        source(sink);
        if (fsync(uniqueTempFd.get()) < 0) {
            unlink(tempPath.c_str());
            return false;
        }

        UniqueFd nextWriteFd(openWriteDescriptor(tempPath));
        UniqueFd nextReadFd(openReadDescriptor(tempPath));

        std::string parentPath = std::filesystem::path(path_).parent_path();
        if (parentPath.empty()) {
            parentPath = ".";
        }
        int parentFd = open(parentPath.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (parentFd < 0) {
            unlink(tempPath.c_str());
            return false;
        }
        UniqueFd uniqueParentFd(parentFd);

        if (rename(tempPath.c_str(), path_.c_str()) < 0) {
            unlink(tempPath.c_str());
            return false;
        }
        renamed = true;

        if (fsync(uniqueParentFd.get()) < 0) {
            throw StorageError("Failed to sync database file");
        }

        writeFd_.reset(nextWriteFd.release());
        readFd_.reset(nextReadFd.release());
        return true;
    } catch (const StorageError&) {
        if (!renamed) {
            unlink(tempPath.c_str());
            return false;
        }
        throw;
    } catch (...) {
        if (!renamed) {
            unlink(tempPath.c_str());
        }
        throw;
    }
}
