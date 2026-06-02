#pragma once
#include <stdexcept>
#include <string>

class NanoStoreException : public std::runtime_error
{
public:
    explicit NanoStoreException(const std::string &msg)
        : std::runtime_error(msg) {}
};

class WalCorruptionException : public NanoStoreException
{
public:
    explicit WalCorruptionException(const std::string &msg)
        : NanoStoreException(msg) {}
};

class KeyNotFoundException : public NanoStoreException
{
public:
    explicit KeyNotFoundException(const std::string &key)
        : NanoStoreException("key not found: " + key) {}
};

class StorageIOException : public NanoStoreException
{
public:
    explicit StorageIOException(const std::string &msg)
        : NanoStoreException(msg) {}
};
