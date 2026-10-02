#include "server/server.h"
#include "server/game.h"
#include "common/logger.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <vector>
#include <memory>

Server::Server(const ServerSettings& settings, Database& db) 
    : port_(settings.port)
    , db_(db)
    , auth_(db_)
    , rooms_(db_, settings.turn_timeout_seconds, settings.max_turn_timeouts)
    , admin_(online_, rooms_, db_, [this] { Stop(); })
    , admin_enabled_(settings.admin_enabled)
    , listen_socket_(INVALID_SOCK)
    , running_(false) {}

Server::~Server() {
    Stop();
}

bool Server::Start() {
    if (!InitNetwork()) {
        LOG_ERROR << "Failed to initialize network";
        return false;
    }

    listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_ == INVALID_SOCK) {
        LOG_ERROR << "Failed to create socket. Error: " << GetLastSocketError();
        return false;
    }

    int opt = 1;
    if (setsockopt(
            listen_socket_, 
            SOL_SOCKET, 
            SO_REUSEADDR, 
            reinterpret_cast<const char*>(&opt), 
            sizeof(opt)
        ) < 0
    ) {
        LOG_ERROR << "setsockopt failed";
        CloseSocket(listen_socket_);
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        LOG_ERROR << "Bind failed. Error: " << GetLastSocketError();
        LOG_ERROR << "Port " << port_ << " might be in use";
        CloseSocket(listen_socket_);
        return false;
    }

    if (listen(listen_socket_, 10) < 0) {
        LOG_ERROR << "Listen failed";
        CloseSocket(listen_socket_);
        return false;
    }

    LOG_INFO << "Server listening on port " << port_ << "...";
    running_ = true;
    rooms_.StartTurnWatchdog();
    if (admin_enabled_)
        admin_.Start();
    return true;
}

void Server::Stop() {
    rooms_.StopTurnWatchdog();
    if (listen_socket_ != INVALID_SOCK) {
        CloseSocket(listen_socket_);
        listen_socket_ = INVALID_SOCK;
    }
    running_ = false;
    CleanupNetwork();
}

void Server::Run() {
    if (!running_) {
        LOG_ERROR << "Server not started";
        return;
    }

    LOG_INFO << "Waiting for connection...";
    
    while (running_) {
        SocketType client_socket = AcceptClient();
        if (client_socket == INVALID_SOCK) {
            if (running_) {
                LOG_ERROR << "Accept failed";
            }
            continue;
        }
        LOG_INFO << "Client connected!";
        std::thread client_thread(&Server::HandleClient, this, client_socket);
        client_thread.detach();
    }
}

SocketType Server::AcceptClient() {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    return accept(listen_socket_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
}

void Server::HandleClient(SocketType client_socket) {
    auto session = std::make_shared<Session>(client_socket);

    MessageWriter welcome(MessageType::GAME_START);
    welcome.WriteUInt8(PROTOCOL_VER);
    session->SendSessionMessage(welcome.Finish());

    session->SetMessageHandler([this, session](MessageType t, const std::vector<uint8_t>& p) {
        HandleAuthMessage(session, t, p); 
    });

    session->Run();

    rooms_.OnWaitingPlayerDisconnected(session);
    if (auto match = rooms_.GetMatch(session->GetId())) {
        match->game->OnPlayerDisconnect(match->player_number);
        rooms_.RemoveMatch(session);
    }
    online_.Release(session->GetUserId(), session);

    LOG_INFO << "[Server] Client handler finished";
}

void Server::HandleAuthMessage(std::shared_ptr<Session> session, MessageType type, const std::vector<uint8_t>& payload) {
    if (type != MessageType::AUTH_REGISTER && type != MessageType::AUTH_LOGIN) {
        SendAuthFail(session, static_cast<uint8_t>(AuthError::AUTH_REQUIRED));
        return;
    }

    std::string name, pass;
    if (!AuthService::ParseCredentials(payload, name, pass)) {
        SendAuthFail(session, static_cast<uint8_t>(AuthError::AUTH_REQUIRED));
        return;
    }

    AuthService::Outcome out = (type == MessageType::AUTH_REGISTER)
        ? auth_.Register(name, pass)
        : auth_.Login(name, pass);

    switch(out.result) {
        case AuthService::Result::OK: {
            if (!online_.TryAcquire(out.user.id, session)) {
                LOG_INFO << "[Server] Duplicate login rejected: '" << out.user.username
                          << "' (session " << session->GetId() << ")";
                SendAuthFail(session, static_cast<uint8_t>(AuthError::ALREADY_ONLINE));
                break;
            }

            session->SetUser(out.user.id, out.user.username);

            MessageWriter ok(MessageType::AUTH_OK);
            ok.WriteUInt8(static_cast<uint8_t>(out.user.username.size()));
            ok.WriteBytes(out.user.username.data(), out.user.username.size());
            ok.WriteUInt32(static_cast<uint32_t>(out.user.wins));
            ok.WriteUInt32(static_cast<uint32_t>(out.user.losses));
            session->SendSessionMessage(ok.Finish());

            LOG_INFO << "[Server] User '" << out.user.username
                      << "' authenticated (session " << session->GetId() << ")";
            
            session->SetMessageHandler([this, session](MessageType t, const std::vector<uint8_t>& p) {
                HandleLobbyMessage(session, t, p);
            });
            break;
        }
        case AuthService::Result::BAD_CREDENTIALS:
            SendAuthFail(session, static_cast<uint8_t>(AuthError::BAD_CRIDENTIALS));
            break;
        case AuthService::Result::USERNAME_TAKEN:
            SendAuthFail(session, static_cast<uint8_t>(AuthError::USERNAME_TAKEN));
            break;
        case AuthService::Result::USERNAME_INVALID:
            SendAuthFail(session, static_cast<uint8_t>(AuthError::USERNAME_INVALID));
            break;
        case AuthService::Result::INVALID_PAYLOAD:
            SendAuthFail(session, static_cast<uint8_t>(AuthError::INVALID_PAYLOAD));
            break;
    }
}

void Server::HandleLobbyMessage(std::shared_ptr<Session> session, MessageType type, const std::vector<uint8_t>& payload) {
    switch (type) {
        case MessageType::ROOM_LIST_REQUEST:
            session->SendSessionMessage(rooms_.BuildRoomListPayload());
            break;
        case MessageType::ROOM_CREATE: {
            if (payload.size() < 2) return;
            const uint8_t name_len = payload[0];
            if (payload.size() < size_t(1) + name_len + 1) return;
            std::string name(payload.begin() + 1, payload.begin() + 1 + name_len);
            const bool is_private = payload[1 + name_len] != 0;
            if (name.empty() || name.size() > 24) return;

            const uint16_t id = rooms_.CreateRoom(session, name, is_private);
            if (id == 0) {
                MessageWriter fail(MessageType::ROOM_JOIN_FAIL);
                fail.WriteUInt8(0x03);
                session->SendSessionMessage(fail.Finish());
                break;
            }
            MessageWriter created(MessageType::ROOM_CREATED);
            created.WriteUInt16(id);
            session->SendSessionMessage(created.Finish());
            break;
        }
        case MessageType::ROOM_JOIN: {
            if (payload.size() < 2) return;
            const uint16_t id = static_cast<uint16_t>(payload[0] | (payload[1] << 8));
            const auto res = rooms_.JoinRoom(session, id);
            if (res != RoomManager::JoinResult::OK) {
                MessageWriter fail(MessageType::ROOM_JOIN_FAIL);
                fail.WriteUInt8(static_cast<uint8_t>(res));
                session->SendSessionMessage(fail.Finish());
            }
            break;
        }
        case MessageType::HISTORY_REQUEST: {
            const uint8_t limit = payload.empty() ? 10 : payload[0];
            session->SendSessionMessage(BuildHistoryPayload(*session, limit));
            break;
        }

        default:
            LOG_WARN << "[Server] Lobby ignoring message type "
                      << static_cast<int>(type);
            break;
    }
}

void Server::SendAuthFail(const std::shared_ptr<Session>& session, uint16_t code) {
    MessageWriter fail(MessageType::AUTH_FAIL);
    fail.WriteUInt8(code);
    session->SendSessionMessage(fail.Finish());
}

std::vector<uint8_t> Server::BuildHistoryPayload(const Session &session, uint8_t limit) {
    const auto records = db_.GetUserHistory(session.GetUserId(), limit);

    MessageWriter w(MessageType::HISTORY);
    w.WriteUInt8(static_cast<uint8_t>(records.size()));

    for (const auto& r : records) {
        const bool win = (r.winner_id == session.GetUserId());
        const std::string& opponent = (r.player1_id == session.GetUserId()) ? r.player2_name : r.player1_name;

        w.WriteUInt8(win ? 1 : 0);
        w.WriteUInt8(static_cast<uint8_t>(opponent.size()));
        w.WriteBytes(opponent.data(), opponent.size());
        w.WriteUInt8(static_cast<uint8_t>(r.finished_at.size()));
        w.WriteBytes(r.finished_at.data(), r.finished_at.size());
    }
    return w.Finish();
}
