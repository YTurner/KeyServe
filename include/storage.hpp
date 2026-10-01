#pragma once

#include "UniqueFd.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

enum class RecordType : std::uint8_t { Put = 1, Delete = 2 };
struct Record {
    RecordType type;
    std::string key;
    std::string value;
};

using EntrySink = std::function<void(const std::string& key, const std::string& value)>;
using EntrySource = std::function<void(const EntrySink& sink)>;

class Storage {
  public:
    explicit Storage(const std::string& path);

    void appendPut(const std::string& key, const std::string& value);
    void appendDelete(const std::string& key);

    void replay(const std::function<void(const Record&)>& apllyRecord);
    bool compact(const EntrySource& source);

  private:
    std::string path_;
    // Kept open for the Storage lifetime to avoid reopening the DB for every append.
    UniqueFd writeFd_;
    UniqueFd readFd_;

    int initializeFile();
    int validateFile() const;
    void recoverIncompleteTail(off_t lastValidLength);
};