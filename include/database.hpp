#include <optional>
#include <string>
#include <unordered_map>

class Database
{
public:
    void put(const std::string &key, const std::string &value);
    std::optional<std::string> get(const std::string &key) const;
    bool remove(const std::string &key);
    bool exists(const std::string &key) const;

private:
    std::unordered_map<std::string, std::string> data_;
};