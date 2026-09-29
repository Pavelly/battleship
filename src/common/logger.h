#pragma once

#include <cstddef>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

namespace logging {

enum class Level : int { Debug = 0, Info = 1, Warn = 2, Error = 3 };

struct Config {
    std::string path = "logs/server.log";
    Level file_level = Level::Debug;
    Level console_level = Level::Warn;
    size_t max_file_size = 5 * 1024 * 1024;
    int keep_backups = 3;
};

Level ParseLevel(const std::string& name, Level def);

class Logger {
public:
    static Logger& Instance();

    bool Init(const Config& cfg);
    void Write(Level lvl, const std::string& message);
    void Shutdown();
private:
    Logger() = default;
    void RotateUnlocked();
    std::string FormatHeader(Level lvl);

    std::mutex mutex_;
    Config cfg_;
    std::ofstream file_;
    bool ready_ = false;
    size_t written_ = 0;
};

class LogStream {
public:
    explicit LogStream(Level lvl) : level_(lvl) {}
    ~LogStream() { Logger::Instance().Write(level_, buffer_.str()); }

    template <typename T>
    LogStream& operator<<(const T& value) {
        buffer_ << value;
        return *this;
    }
private:
    Level level_;
    std::ostringstream buffer_;
};

}

#define LOG_DEBUG ::logging::LogStream(::logging::Level::Debug)
#define LOG_INFO ::logging::LogStream(::logging::Level::Info)
#define LOG_WARN ::logging::LogStream(::logging::Level::Warn)
#define LOG_ERROR ::logging::LogStream(::logging::Level::Error)