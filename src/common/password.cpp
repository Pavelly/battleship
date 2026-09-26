#include "common/password.h"
#include "common/sha256.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace auth {

std::string GenerateSalt() {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 255);

    std::ostringstream ss;
    for (int i = 0; i < 16; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << dist(rd);
    return ss.str();
}

std::string HashPassword(const std::string& password, const std::string& salt) {
    const std::string combined = salt + password;
    return Sha256::HashHex(combined);
}

bool VerifyPassword(const std::string& password,
                    const std::string& salt,
                    const std::string& expected_hash_hex) {
    const std::string actual = HashPassword(password, salt);

    if (actual.size() != expected_hash_hex.size())
        return false;

    unsigned char diff = 0;
    for (size_t i = 0; i < actual.size(); ++i)
        diff |= static_cast<unsigned char>(actual[i] ^ expected_hash_hex[i]);
    return diff == 0;
}

}