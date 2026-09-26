#pragma once

#include <stdexcept>
#include <string>

// StorageError: OS/file access failures.
// CorruptionError: database bytes violate the file format.
// IncompleteRecordError: recoverable truncated final record after an interrupted append.
class KeyServeError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class StorageError : public KeyServeError {
  public:
    using KeyServeError::KeyServeError;
};

class CorruptionError : public KeyServeError {
  public:
    using KeyServeError::KeyServeError;
};

class IncompleteRecordError : public CorruptionError {
  public:
    using CorruptionError::CorruptionError;
};