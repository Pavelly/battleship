#include "common/logger.h"
#include <chrono>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <thread>
#include "logger.h"

namespace logging {

namespace {
const char* LevelName(Level lvl) {
    switch (lvl) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
    }
    return "?   ";
}
}

Logger& Logger::Instance() {
    static Logger* instance = new Logger();
    return *instance;
}

bool Logger::Init(const Config& cfg) {
    std::lock_guard<std::mutex> lock(mutex_);
    cfg_ = cfg;

    std::error_code ec;
    const std::filesystem::path p(cfg_.path);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path(), ec);
    
    if (std::filesystem::exists(p, ec))
        written_ = static_cast<size_t>(std::filesystem::file_size(p, ec));

    file_.open(cfg_.path, std::ios::app);
    ready_ = file_.is_open();
    return ready_;
}

void Logger::Write(Level lvl, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string line = FormatHeader(lvl) + message;

    if (ready_ && lvl >= cfg_.file_level) {
        if (written_ >= cfg_.max_file_size)
            RotateUnlocked();
        file_ << line << std::endl;
        file_.flush();
        written_ += line.size() + 1;    
    }

    if (lvl >= cfg_.console_level)
        std::cout << line << std::endl;
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open())
        file_.close();
    ready_ = false;
}

void Logger::RotateUnlocked() {
    file_.close();
    std::error_code ec;
    for (int i = cfg_.keep_backups - 1; i >= 1; --i) {
        const auto src = cfg_.path + "." + std::to_string(i);
        const auto dst = cfg_.path + "." + std::to_string(i + 1);
        if (std::filesystem::exists(src, ec))
            std::filesystem::rename(src, dst, ec);
    }
    std::filesystem::rename(cfg_.path, cfg_.path + ".1", ec);
    file_.open(cfg_.path, std::ios::app);
    ready_ = file_.is_open();
    written_ = 0;
}

std::string Logger::FormatHeader(Level lvl)
{
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif

    std::ostringstream os;
    os << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms
       << " [" << LevelName(lvl) << "] "
       << "[tid:" << std::hex << std::setw(4) << std::setfill('0')
       << (std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFF)
       << std::dec << "] ";
    return os.str();
}

Level ParseLevel(const std::string& name, Level def) {
    std::string s;
    s.reserve(name.size());
    for (char c : name) s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (s == "debug") return Level::Debug;
    if (s == "info") return Level::Info;
    if (s == "warn") return Level::Warn;
    if (s == "error") return Level::Error; 
    return def;   
}

}