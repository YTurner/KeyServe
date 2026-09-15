#include "database.hpp"

void Database::put(const std::string &key, const std::string &value)
{
    data_.insert_or_assign(key, value);
}

std::optional<std::string> Database::get(const std::string &key) const
{
    auto it = data_.find(key);

    if (it == data_.end())
    {
        return std::nullopt;
    }

    return it->second;
}

bool Database::remove(const std::string &key)
{
    return data_.erase(key) > 0;
}

bool Database::exists(const std::string &key) const
{
    return data_.find(key) != data_.end();
}