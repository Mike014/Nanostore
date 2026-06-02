#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>
#include "exceptions.hpp"

enum class OperationType : uint8_t
{
    SET = 0,
    DEL = 1
};

struct WalEntry
{
    OperationType operation;
    std::string key;
    std::string value;
};

class WAL
{
public:
    void append(const WalEntry &entry);
    std::vector<WalEntry> recover();
    void compact(const std::unordered_map<std::string, std::string> &index);

    WAL(const std::string &path);
    ~WAL();

private:
    static constexpr uint32_t WAL_MAGIC = 0x4157534E; // "NSWA"
    static constexpr uint8_t WAL_VERSION = 0x01;

    std::string path_;
    std::ofstream file_;

    static uint32_t crc32(const uint8_t *data, size_t len);
    uint32_t entry_crc32(const WalEntry &entry);
    void write_entry(std::ofstream &stream, const WalEntry &entry);
};
