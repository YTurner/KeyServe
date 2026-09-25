#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class RecordType : std::uint8_t { Put = 1, Delete = 2 };

struct Record {
    RecordType type;
    std::string key;
    std::string value;
};

class Storage {
  public:
    explicit Storage(const std::string& path);

    void appendPut(const std::string& key, const std::string& value) const;
    void appendDelete(const std::string& key) const;

    std::vector<Record> readAll() const;

  private:
    std::string path_;
    void initializeFile();
    void validateFile() const;
};