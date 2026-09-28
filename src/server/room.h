#pragma once

#include "net/session.h"
#include <cstdint>
#include <memory>
#include <string>

class Game;

enum class RoomState : uint8_t {
    WAITING = 0,
    IN_GAME = 0x01
};

class Room {
public:
    Room(uint16_t id, std::string name, bool is_private, std::shared_ptr<Session> owner)
        : id_(id), name_(std::move(name)), private_(is_private), owner_(std::move(owner)) {}

    uint16_t GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    bool IsPrivate() const { return private_; }
    RoomState GetState() const { return state_; }
    const std::shared_ptr<Session>& GetOwner() const { return owner_; }
    const std::shared_ptr<Session>& GetGuest() const { return guest_; }
    const std::shared_ptr<Game>& GetGame() const { return game_; }
    bool HasPlayer(const std::shared_ptr<Session>& s) const { return owner_ == s || guest_ == s; }

    void SetGuest(std::shared_ptr<Session> guest) {
        guest_ = std::move(guest);
        state_ = RoomState::IN_GAME;
    }
    void SetGame(std::shared_ptr<Game> game) { game_ = std::move(game); }
private:
    uint16_t id_;
    std::string name_;
    bool private_;
    RoomState state_ = RoomState::WAITING;
    std::shared_ptr<Session> owner_;
    std::shared_ptr<Session> guest_;
    std::shared_ptr<Game> game_;
};