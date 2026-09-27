#include "server/server.h"
#include "db/database.h"
#include <iostream>

int main() {
    const uint16_t PORT = 9090;

    Database db;
    if (!db.Open("battleship.db")) {
        std::cerr << "Failed to open database\n";
        return 1;
    }

    Server server(PORT, db);
    if (!server.Start()) {
        std::cerr << "Failed to start server\n";
        return 1;
    }

    server.Run();
    return 0;
}