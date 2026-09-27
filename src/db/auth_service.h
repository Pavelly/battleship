#pragma once

#include "db/database.h"
#include <cstdint>
#include <string>
#include <vector>

class AuthService {
public:
    enum class Result {
        OK,
        BAD_CREDENTIALS,
        USERNAME_TAKEN,
        USERNAME_INVALID,
        INVALID_PAYLOAD
    };

    struct Outcome {
        Result result = Result::INVALID_PAYLOAD;
        UserRecord user;
    };

    explicit AuthService(Database& db);
    Outcome Register(const std::string& username, const std::string& password);
    Outcome Login(const std::string& username, const std::string& password);

    static bool ParseCredentials(const std::vector<uint8_t>& payload,
                                 std::string& username,
                                 std::string& password);
    static bool IsUsernameValid(const std::string& username);
private:
    Database& db_;
};