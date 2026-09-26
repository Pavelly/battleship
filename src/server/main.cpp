#include "server/server.h"
#include <iostream>

int main() {
    const uint16_t PORT = 9090;

    Server server(PORT);

    if (!server.Start()) {
        std::cerr << "Failed to start server\n";
        return 1;
    }

    server.Run();

    return 0;
}