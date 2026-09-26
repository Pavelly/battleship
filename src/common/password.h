#pragma once

#include <string>

namespace auth {

std::string GenerateSalt();
std::string HashPassword(const std::string& password, const std::string& salt);
bool VerifyPassword(const std::string& password,
                    const std::string& salt,
                    const std::string& expected_hash_hex);

}