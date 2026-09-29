#include "common/logger.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

int main() {
    namespace fs = std::filesystem;
    fs::remove_all("test_logs");

    logging::Config cfg;
    cfg.path = "test_logs/logger_test.log";
    cfg.file_level = logging::Level::Info;
    cfg.console_level = logging::Level::Error;
    cfg.max_file_size = 400;
    cfg.keep_backups = 2;
    assert(logging::Logger::Instance().Init(cfg));

    LOG_DEBUG << "invincible debug";
    LOG_INFO << "hello logger";
    LOG_WARN << "warning message";
    LOG_ERROR << "error message";

    {
        std::ifstream in(cfg.path);
        const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(content.find("hello logger") != std::string::npos);
        assert(content.find("warning message") != std::string::npos);
        assert(content.find("error message") != std::string::npos);
        assert(content.find("invincible debug") == std::string::npos);
        assert(content.find("[INFO ]") != std::string::npos);
        std::cout << "Levels & format: OK\n";
    }

    for (int i = 0; i < 50; ++i)
        LOG_INFO << "rotation line " << i;
    assert(fs::exists(cfg.path + ".1"));
    std::cout << "Rotation: OK\n";

    logging::Logger::Instance().Shutdown();
    fs::remove_all("test_logs");
    std::cout << "All logger tests passed!\n";
    return 0;
}