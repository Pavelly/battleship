#include "common/config.h"
#include "common/logger.h"
#include <algorithm>
#include <cctype>
#include <fstream>

namespace config {

namespace {

std::string Trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string MakeKey(const std::string& section, const std::string& key) {
    return ToLower(Trim(section)) + "." + ToLower(Trim(key));
}

}

bool Config::Load(const std::string &path)
{
    std::ifstream in(path);
    if (!in.is_open())
        return false;

    values_.clear();
    std::string line, section;

    while (std::getline(in, line)) {
        const std::string t = Trim(line);
        if (t.empty() || t[0] == ';' || t[0] == '#')
            continue;
        if (t.front() == '[' && t.back() == ']') {
            section = ToLower(Trim(t.substr(1, t.size() - 2)));
            continue;
        }
        const auto eq = t.find('=');
        if (eq == std::string::npos)
            continue;
        
        const std::string key = ToLower(Trim(t.substr(0, eq)));
        const std::string value = Trim(t.substr(eq + 1));
        if (!key.empty())
            values_[section + "." + key] = value;
    }
    return true;
}

std::string Config::GetString(const std::string& section, const std::string& key, const std::string& def) const {
    const auto it = values_.find(MakeKey(section, key));
    return it == values_.end() ? def : it->second;
}

int Config::GetInt(const std::string& section, const std::string& key, int def) const {
    const std::string raw = GetString(section, key, "");
    if (raw.empty())
        return def;
    try {
        return std::stoi(raw);
    } catch (...) {
        LOG_WARN << "[Config] Bad integer at " << section << "." << key
                 << " = '" << raw << "', using default " << def;
        return def;
    }
}

bool Config::GetBool(const std::string& section, const std::string& key, bool def) const {
    const std::string raw = ToLower(GetString(section, key, ""));
    if (raw.empty())
        return def;
    if (raw == "true" || raw == "1" || raw == "yes" || raw == "on")
        return true;
    if (raw == "false" || raw == "0" || raw == "no" || raw == "off")
        return false;
    LOG_WARN << "[Config] Bad boolean at " << section << "." << key
             << " = '" << raw << "', using default " << (def ? "true" : "false");
    return def;
}

bool Config::SaveDefault(const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open())
        return false;

    out <<
        "# Battleship server configuration\n"
        "[server]\n"
        "port = 9090\n"
        "\n"
        "[database]\n"
        "path = battleship.db\n"
        "\n"
        "[log]\n"
        "path = logs/server.log\n"
        "; levels: debug, info, warn, error\n"
        "file_level = debug\n"
        "console_level = warn\n"
        "max_file_size = 5242880\n"
        "keep_backups = 3\n"
        "\n"
        "[game]\n"
        "turn_timeout_seconds = 30\n"
        "max_turn_timeouts = 3\n"
        "\n"
        "[admin]\n"
        "enabled = true\n";
    return true;
}

}