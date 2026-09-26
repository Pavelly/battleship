#pragma once

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>

    using SocketType = SOCKET;
    constexpr SocketType INVALID_SOCK = INVALID_SOCKET;

    inline void CloseSocket(SocketType sock) {
        closesocket(sock);
    }

    inline int GetLastSocketError() {
        return WSAGetLastError();
    }

    inline bool InitNetwork() {
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
    }

    inline void CleanupNetwork() {
        WSACleanup();
    }

    #define SOCK_ERROR SOCKET_ERROR
    #define MSG_NOSIGNAL 0

#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>

    using SocketType = int;
    constexpr SocketType INVALID_SOCK = -1;

    inline void CloseSocket(SocketType sock) {
        close(sock);
    }

    inline int GetLastSocketError() {
        return errno;
    }

    inline bool InitNetwork() {
        return true;
    }

    inline void CleanupNetwork() {}

    #define SOCK_ERROR -1
#endif
