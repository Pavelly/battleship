#include "common/config.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    namespace fs = std::filesystem;
    fs::remove_all("test_cfg");
    fs::create_directories("test_cfg");

    {
        std::ofstream out("test_cfg/sample.ini");
        out << "; comment\n"
               "\n"
               "[server]\n"
               "port = 1234\n"
               "\n"
               "[log]\n"
               "file_level = info\n"
               "path = my/log.log\n"
               "\n"
               "[game]\n"
               "turn_timeout_seconds = 7\n"
               "bad_int = abc\n"
               "\n"
               "[flags]\n"
               "enabled = yes\n"
               "disabled = off\n";
    }

    config::Config cfg;
    assert(cfg.Load("test_cfg/sample.ini"));

    assert(cfg.GetString("server", "port", "x") == "1234");
    assert(cfg.GetInt("server", "port", 0) == 1234);
    assert(cfg.GetInt("game", "bad_int", 42) == 42);          // битое значение → дефолт
    assert(cfg.GetString("log", "path", "") == "my/log.log");
    assert(cfg.GetString("log", "console_level", "warn") == "warn");  // нет ключа → дефолт
    assert(cfg.GetBool("flags", "enabled", false) == true);
    assert(cfg.GetBool("flags", "disabled", true) == false);
    assert(cfg.GetBool("flags", "missing", true) == true);
    std::cout << "Parsing & defaults: OK\n";

    assert(!cfg.Load("test_cfg/nope.ini"));                   // отсутствующий файл
    std::cout << "Missing file handled: OK\n";

    assert(config::Config::SaveDefault("test_cfg/gen.ini"));
    config::Config gen;
    assert(gen.Load("test_cfg/gen.ini"));
    assert(gen.GetInt("server", "port", 0) == 9090);
    assert(gen.GetInt("game", "max_turn_timeouts", 0) == 3);
    std::cout << "Template round-trip: OK\n";

    fs::remove_all("test_cfg");
    std::cout << "All config tests passed!\n";
    return 0;
}
