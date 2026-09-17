#pragma once

#include <stdexcept>
#include <string>

class KeyServeError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class StorageError : public KeyServeError
{
public:
    using KeyServeError::KeyServeError;
};

class CorruptionError : public KeyServeError
{
public:
    using KeyServeError::KeyServeError;
};