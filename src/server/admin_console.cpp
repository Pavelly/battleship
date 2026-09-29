#include "admin_console.h"
#include <iostream>
#include <sstream>

namespace {

std::string Trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

std::string PhaseName(int phase) {
    switch (phase) {
        case 0: return "PLACEMENT";
        case 1: return "BATTLE";
        case 2: return "FINISHED";
        default: return "?";
    }
}

}

AdminConsole::AdminConsole(const OnlineRegistry& online,
                           const RoomManager& rooms,
                           Database& db,
                           std::function<void()> shutdown_cb)
    : online_(online)
    , rooms_(rooms)
    , db_(db)
    , shutdown_cb_(std::move(shutdown_cb)) {
}

AdminConsole::~AdminConsole() {
    running_.store(false);
}

void AdminConsole::Start() {
    if (running_.load())
        return;
    running_.store(true);
    start_time_ = std::chrono::steady_clock::now();
    thread_ = std::thread(&AdminConsole::Loop, this);
    thread_.detach();
    std::cout << "[Admin] Console started. Type 'help' for commands\n";
}

void AdminConsole::Loop() {
    std::string line;
    while (running_.load()) {
        std::cout << "[server]> " << std::flush;
        if (!std::getline(std::cin, line))
            break; 
        HandleCommand(Trim(line));
    }
}

void AdminConsole::HandleCommand(const std::string& line) {
    if (line.empty())
        return;
    std::istringstream ss(line);
    std::string cmd, arg;
    ss >> cmd >> arg;

    if      (cmd == "help")     CmdHelp();
    else if (cmd == "status")   CmdStatus();
    else if (cmd == "rooms")    CmdRooms();
    else if (cmd == "online")   CmdOnline();
    else if (cmd == "games")    CmdGames();
    else if (cmd == "kick")     CmdKick(arg);
    else if (cmd == "shutdown") CmdShutdown();
    else std::cout << "Unknown command. Type 'help'.\n";
}

void AdminConsole::CmdHelp() {
    std::cout <<
        "Available commands:\n"
        "  status           summary: uptime, online, rooms, games in DB\n"
        "  online           list of online users\n"
        "  rooms            list of rooms (waiting / in game)\n"
        "  games            active games with phase and turn\n"
        "  kick <session>   disconnect a session by its id\n"
        "  shutdown         stop the server gracefully\n"
        "  help             this list\n";
}

void AdminConsole::CmdStatus() {
    const auto now  = std::chrono::steady_clock::now();
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    const auto online = online_.Snapshot();
    const auto rooms  = rooms_.SnapshotRooms();

    size_t waiting = 0, in_game = 0;
    for (const auto& r : rooms) {
        if (r.in_game) ++in_game;
        else           ++waiting;
    }

    std::cout << "=== SERVER STATUS ===\n"
              << "Uptime:        " << (secs / 60) << " min " << (secs % 60) << " s\n"
              << "Online users:  " << online.size() << "\n"
              << "Rooms:         " << rooms.size()
              << " (waiting: " << waiting << ", in game: " << in_game << ")\n"
              << "Games in DB:   " << db_.CountGames() << "\n";
}

void AdminConsole::CmdOnline() {
    const auto online = online_.Snapshot();
    std::cout << "=== ONLINE (" << online.size() << ") ===\n";
    for (const auto& u : online)
        std::cout << "user #" << u.user_id << "  " << u.username
                  << "  (session " << u.session_id << ")\n";
    if (online.empty())
        std::cout << "(nobody)\n";
}

void AdminConsole::CmdRooms() {
    const auto rooms = rooms_.SnapshotRooms();
    std::cout << "=== ROOMS (" << rooms.size() << ") ===\n";
    for (const auto& r : rooms) {
        std::cout << "#" << r.id << "  \"" << r.name << "\"  "
                  << (r.is_private ? "private" : "public ") << "  "
                  << (r.in_game ? "IN_GAME" : "WAITING")
                  << "  owner: " << r.owner_name;
        if (!r.guest_name.empty())
            std::cout << ", guest: " << r.guest_name;
        std::cout << "\n";
    }
    if (rooms.empty())
        std::cout << "(empty)\n";
}

void AdminConsole::CmdGames() {
    const auto games = rooms_.SnapshotGames();
    std::cout << "=== ACTIVE GAMES (" << games.size() << ") ===\n";
    for (const auto& g : games)
        std::cout << "room #" << g.room_id << ": " << g.player1 << " vs " << g.player2
                  << " | phase: " << PhaseName(g.phase)
                  << " | turn: player " << g.current_turn << "\n";
    if (games.empty())
        std::cout << "(none)\n";
}

void AdminConsole::CmdKick(const std::string& arg) {
    if (arg.empty()) {
        std::cout << "Usage: kick <session_id>\n";
        return;
    }
    int id = 0;
    try {
        id = std::stoi(arg);
    } catch (...) {
        std::cout << "Invalid session id\n";
        return;
    }
    auto session = online_.FindSession(id);
    if (!session) {
        std::cout << "Session " << id << " not found among online users\n";
        return;
    }
    std::cout << "Kicking session " << id << " (" << session->GetUsername() << ")\n";
    session->Kick();
}

void AdminConsole::CmdShutdown() {
    std::cout << "[Admin] Shutdown requested, stopping server...\n";
    running_.store(false);
    if (shutdown_cb_)
        shutdown_cb_();
}