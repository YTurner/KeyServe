#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class RecordType : std::uint8_t { Put = 1, Delete = 2 };

// Key Serve DataBase magic bytes for db file signature
constexpr char FILE_MAGIC[4] = {'K', 'S', 'D', 'B'};
constexpr std::uint8_t FILE_VERSION = 1;

constexpr std::uint32_t MAX_KEY_SIZE = 1024 * 1024;         // 1 MiB
constexpr std::uint32_t MAX_VALUE_SIZE = 64 * 1024 * 1024;  // 64 MiB

struct Record {
    RecordType type;
    std::string key;
    std::string value;
};

class Storage {
   public:
    explicit Storage(const std::string& path);

    void appendPut(const std::string& key, const std::string& value);
    void appendDelete(const std::string& key);

    std::vector<Record> readAll() const;

   private:
    std::string path_;
    void initilizeFile();
    void validateFile() const;
};