#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <span>

constexpr uint8_t PROTOCOL_VER = 0x03;

enum class MessageType : uint8_t {
    SHOT                    = 0x01,
    SHOT_RESULT             = 0x02,
    PLACE_SHIPS             = 0x03,
    GAME_START              = 0x04,
    GAME_OVER               = 0x05,

    WAITING                 = 0x0A,
    MATCH_FOUND             = 0x0B,
    PLAYER_NUMBER           = 0x0C,

    YOUR_TURN               = 0x14,
    ENEMY_TURN              = 0x15,
    ENEMY_SHOT              = 0x16,
    PLACEMENT_READY         = 0x17,
    BATTLE_START            = 0x18,
    OPPONENT_DISCONNECTED   = 0x19,

    AUTH_REGISTER           = 0x1E,
    AUTH_LOGIN              = 0x1F,
    AUTH_OK                 = 0x20,
    AUTH_FAIL               = 0x21,

    PLACE_RANDOM            = 0x22,
    OWN_SHIPS               = 0x23,

    ERROR_MSG               = 0xFF
};

enum class AuthError : uint8_t {
    BAD_CRIDENTIALS     = 0x01,
    USERNAME_TAKEN      = 0x02,
    INVALID_PAYLOAD     = 0x03,
    AUTH_REQUIRED       = 0x04,
    USERNAME_INVALID    = 0x05,
    ALREADY_ONLINE      = 0x06
};

enum class ShotResult : uint8_t {
    MISS            = 0,
    HIT             = 0x01,
    SINK            = 0x02,
    ALREADY_SHOT    = 0x03
};

class MessageWriter {
public:
    MessageWriter(MessageType type);

    void WriteUInt8(uint8_t value);
    void WriteUInt16(uint16_t value);
    void WriteUInt32(uint32_t value);
    void WriteBytes(const void* data, size_t size);

    std::vector<uint8_t> Finish();
private:
    MessageType type_;
    std::vector<uint8_t> payload_;
};

class MessageReader {
public:
    explicit MessageReader(const std::vector<uint8_t>& data);

    std::optional<uint8_t> ReadUInt8();
    std::optional<uint16_t> ReadUInt16();
    std::optional<uint32_t> ReadUInt32();
    std::optional<std::vector<uint8_t>> ReadBytes(size_t count);

    MessageType GetType() const { return type_; }

    bool HasMore() const { return offset_ < data_.size(); }
private:
    MessageType type_;
    std::vector<uint8_t> data_;
    size_t offset_;
};

namespace Protocol {
    size_t TryReadMessage(const std::vector<uint8_t>& buffer, MessageType& type, std::vector<uint8_t>& payload);
    void WriteUInt16BE(std::vector<uint8_t>& buffer, uint16_t value);
    uint16_t ReadUInt16BE(const uint8_t* data);
}