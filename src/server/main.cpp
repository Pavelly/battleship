#include "common/config.h"
#include "common/logger.h"
#include "server/server.h"
#include "db/database.h"
#include <iostream>

int main(int argc, char** argv) {
    const std::string config_path = (argc > 1) ? argv[1] : "server.ini";

    config::Config cfg;
    if (!cfg.Load(config_path)) {
        std::cerr << "Config '" << config_path
                  << "' not found starting with defaults, template written\n";
        config::Config::SaveDefault(config_path);
    }

    logging::Config log_cfg;
    log_cfg.path            = cfg.GetString("log", "path", "logs/server.log");
    log_cfg.file_level      = logging::ParseLevel(cfg.GetString("log", "file_level", "debug"), logging::Level::Debug);
    log_cfg.console_level   = logging::ParseLevel(cfg.GetString("log", "console_level", "warn"), logging::Level::Warn);
    log_cfg.max_file_size   = static_cast<size_t>(cfg.GetInt("log", "max_file_size", 5 * 1024 * 1024));
    log_cfg.keep_backups    = cfg.GetInt("log", "keep_backups", 3);

    if (!logging::Logger::Instance().Init(log_cfg)) {
        std::cerr << "Failed to initialize logger\n";
        return 1;
    }
    LOG_INFO << "[Server] Battleship server starting...";

    Database db;
    if (!db.Open("battleship.db")) {
        LOG_ERROR << "[Server] Failed to open database";
        return 1;
    }

    ServerSettings settings;
    settings.port                   = static_cast<uint16_t>(cfg.GetInt("server", "port", 9090));
    settings.turn_timeout_seconds   = cfg.GetInt("game", "turn_timeout_seconds", 30);
    settings.max_turn_timeouts      = cfg.GetInt("game", "max_turn_timeouts", 3);
    settings.admin_enabled          = cfg.GetBool("admin", "enabled", true);

    Server server(settings, db);
    if (!server.Start()) {
        LOG_ERROR << "[Server] Failed to start server";
        return 1;
    }

    server.Run();

    LOG_INFO << "[Server] Server stopped";
    logging::Logger::Instance().Shutdown();
    return 0;
}