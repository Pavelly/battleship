#include "common/protocol.h"
#include <cstring>
#include <stdexcept>

MessageWriter::MessageWriter(MessageType type)
    : type_(type) {}

void MessageWriter::WriteUInt8(uint8_t value) {
    payload_.push_back(value);
}

void MessageWriter::WriteUInt16(uint16_t value) {
    // Native order
    payload_.push_back(static_cast<uint8_t>(value & 0xFF));
    payload_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void MessageWriter::WriteUInt32(uint32_t value) {
    payload_.push_back(static_cast<uint8_t>(value & 0xFF));
    payload_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));    
    payload_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    payload_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void MessageWriter::WriteBytes(const void *data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    payload_.insert(payload_.end(), bytes, bytes + size);
}

std::vector<uint8_t> MessageWriter::Finish() {
    std::vector<uint8_t> message;
    message.reserve(1 + 2 + payload_.size());

    message.push_back(static_cast<uint8_t>(type_));
    Protocol::WriteUInt16BE(message, static_cast<uint16_t>(payload_.size()));

    message.insert(message.end(), payload_.begin(), payload_.end());

    return message;
}

MessageReader::MessageReader(const std::vector<uint8_t> &data) 
    : data_(data)
    , offset_(0) {
    if (data.size() < 3) {
        throw std::runtime_error("Message too short");
    }

    type_ = static_cast<MessageType>(data[0]);
    uint16_t length = Protocol::ReadUInt16BE(&data[1]);

    if (static_cast<int>(data.size()) != 3 + length) {
        throw std::runtime_error("Message length missmatch");
    }

    offset_ = 3;
}

std::optional<uint8_t> MessageReader::ReadUInt8() {
    if (offset_ >= data_.size())
        return std::nullopt;
    return data_[offset_++];
}

std::optional<uint16_t> MessageReader::ReadUInt16() {
    if (offset_ + 2 > data_.size())
        return std::nullopt;
    uint16_t value = data_[offset_] | (data_[offset_ + 1] << 8);
    offset_ += 2;
    return value;
}

std::optional<uint32_t> MessageReader::ReadUInt32() {
    if (offset_ + 4 > data_.size())
        return std::nullopt;
    uint32_t value = data_[offset_]
                   | (data_[offset_ + 1] << 8)
                   | (data_[offset_ + 2] << 16)
                   | (data_[offset_ + 3] << 24);
    offset_ += 4;
    return value;
}

std::optional<std::vector<uint8_t>> MessageReader::ReadBytes(size_t count) {
    if (offset_ + count > data_.size())
        return std::nullopt;
    std::vector<uint8_t> result(data_.begin() + offset_, data_.begin() + offset_ + count);
    offset_ += count;
    return result;
}

namespace Protocol {
void WriteUInt16BE(std::vector<uint8_t>& buffer, uint16_t value) {
    // Big-endian
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

uint16_t ReadUInt16BE(const uint8_t* data) {
    // Big-endian
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

size_t TryReadMessage(const std::vector<uint8_t>& buffer, MessageType& type, std::vector<uint8_t>& payload) {
    // 3 bytes minimum for header
    if (buffer.size() < 3) {
        return 0;
    }

    type = static_cast<MessageType>(buffer[0]);
    uint16_t length = ReadUInt16BE(&buffer[1]);

    size_t total_size = 3 + length;
    if (buffer.size() < total_size) {
        return 0;
    }

    payload.assign(buffer.begin() + 3, buffer.begin() + 3 + length);

    return total_size;
}
}