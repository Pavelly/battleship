#pragma once

#include <map>
#include <string>

namespace config {

class Config {
public:
    bool Load(const std::string& path);

    std::string GetString(const std::string& section, const std::string& key, const std::string& def) const;
    int GetInt(const std::string& section, const std::string& key, int def) const;
    bool GetBool(const std::string& section, const std::string& key, bool def) const;

    static bool SaveDefault(const std::string& path);
private:
    std::map<std::string, std::string> values_;
};

}