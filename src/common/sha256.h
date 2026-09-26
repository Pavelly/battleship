#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

class Sha256 {
public:
    using Digest = std::array<uint8_t, 32>;

    Sha256();

    void Update(const void* data, size_t len);
    Digest Finalize();

    static Digest Hash(const void* data, size_t len);
    static std::string HashHex(const std::string& data);
    static std::string ToHex(const Digest& digest);
private:
    void ProcessBlock(const uint8_t* p);

    uint32_t state_[8];
    uint64_t bitlen_;
    uint8_t buffer_[64];
    size_t buflen_;
};