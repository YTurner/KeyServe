#pragma once

#include "UniqueFd.hpp"
#include <cstdint>
#include <string>
#include <vector>

enum class RecordType : std::uint8_t { Put = 1, Delete = 2 };

// Logical database operation reconstructed from or written to the storage log.
// Serialization details such as payload length and checksum stay inside Storage.
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

    std::vector<Record> readAll();

  private:
    std::string path_;
    // Kept open for the Storage lifetime to avoid reopening the DB for every append.
    UniqueFd writeFd_;

    void initializeFile();
    void validateFile() const;
    void recoverIncompleteTail(off_t lastValidLength);
};