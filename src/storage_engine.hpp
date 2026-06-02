#pragma once
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>
#include <shared_mutex>
#include <mutex>
#include "wal.hpp"
#include "exceptions.hpp"

class StorageEngine
{
public:
    void set(const std::string &key, const std::string &value);
    std::optional<std::string> get(const std::string &key);
    std::string get_or_throw(const std::string &key);
    void del(const std::string &key);

    bool exists(const std::string &key);
    size_t count() const;
    void clear();
    std::vector<std::string> scan(const std::string& prefix) const;

    StorageEngine(const std::string &path);

    StorageEngine(const StorageEngine&) = delete;
    StorageEngine& operator=(const StorageEngine&) = delete;
    StorageEngine(StorageEngine&&) = delete;
    StorageEngine& operator=(StorageEngine&&) = delete;

private:
    static constexpr size_t MAX_OPS_BEFORE_COMPACT = 10000;
    mutable std::shared_mutex mutex_;

    WAL wal_;
    std::unordered_map<std::string, std::string> index_;
    size_t op_count_ = 0;
};