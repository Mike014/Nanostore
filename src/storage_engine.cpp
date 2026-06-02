#include "storage_engine.hpp"

StorageEngine::StorageEngine(const std::string &path) : wal_(path)
{
    std::vector<WalEntry> vec = wal_.recover();
    for (const WalEntry &entry : vec)
        if (entry.operation == OperationType::SET)
        {
            index_[entry.key] = entry.value;
        }
        else
        {
            index_.erase(entry.key);
        }
}

std::optional<std::string> StorageEngine::get(const std::string &key)
{
    std::shared_lock lock(mutex_);
    auto it = index_.find(key);
    if (it != index_.end())
    {
        return it->second;
    }
    return std::nullopt;
}

std::string StorageEngine::get_or_throw(const std::string &key)
{
    std::shared_lock lock(mutex_);
    auto it = index_.find(key);
    if (it != index_.end())
        return it->second;
    throw KeyNotFoundException(key);
}

void StorageEngine::set(const std::string &key, const std::string &value)
{
    std::unique_lock lock(mutex_);
    wal_.append({OperationType::SET, key, value});
    index_[key] = value;
    op_count_++;
    if (op_count_ >= MAX_OPS_BEFORE_COMPACT)
    {
        wal_.compact(index_);
        op_count_ = 0;
    }
}

void StorageEngine::del(const std::string &key)
{
    std::unique_lock lock(mutex_);
    wal_.append({OperationType::DEL, key, ""});
    index_.erase(key);
    op_count_++;
    if (op_count_ >= MAX_OPS_BEFORE_COMPACT)
    {
        wal_.compact(index_);
        op_count_ = 0;
    }
}

bool StorageEngine::exists(const std::string &key)
{
    std::shared_lock lock(mutex_);
    auto it = index_.find(key);

    return it != index_.end();
}

size_t StorageEngine::count() const
{
    std::shared_lock lock(mutex_);

    return index_.size();
}

void StorageEngine::clear()
{
    std::unique_lock lock(mutex_);
    index_.clear();
    wal_.compact(index_);
    op_count_ = 0;
}

std::vector<std::string> StorageEngine::scan(const std::string &prefix) const
{
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    for (const auto &[key, value] : index_)
        if (key.starts_with(prefix))
        {
            result.push_back(key);
        }
    return result;
}