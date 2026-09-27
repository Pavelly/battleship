#include "db/auth_service.h"
#include "common/password.h"
#include <cctype>

AuthService::AuthService(Database& db)
    : db_(db) {}

AuthService::Outcome AuthService::Register(const std::string& username, const std::string& password) {
    Outcome out;

    if (!IsUsernameValid(username)) {
        out.result = Result::USERNAME_INVALID;
        return out;
    }
    if (password.empty() || password.size() > 64) {
        out.result = Result::INVALID_PAYLOAD;
        return out;
    }

    const std::string salt = auth::GenerateSalt();
    const std::string hash = auth::HashPassword(password, salt);

    int64_t id = 0;
    if (!db_.CreateUser(username, hash, salt, id)) {
        out.result = Result::USERNAME_TAKEN;
        return out;
    }

    auto user = db_.GetUserByName(username);
    if (!user) {
        out.result = Result::USERNAME_TAKEN;
        return out;
    }

    out.user = *user;
    out.result = Result::OK;
    return out;
}

AuthService::Outcome AuthService::Login(const std::string& username, const std::string& password) {
    Outcome out;

    auto user = db_.GetUserByName(username);
    if (!user) {
        out.result = Result::BAD_CREDENTIALS;
        return out;
    }
    if (!auth::VerifyPassword(password, user->salt, user->password_hash)) {
        out.result = Result::BAD_CREDENTIALS;
        return out;
    }

    out.user = *user;
    out.result = Result::OK;
    return out;
}

bool AuthService::ParseCredentials(const std::vector<uint8_t>& payload, std::string& username, std::string& password) {
    if (payload.size() < 2) return false;

    const uint8_t name_len = payload[0];
    if (payload.size() < size_t(1) + name_len + 1) return false;

    const uint8_t pass_len = payload[1 + name_len];
    if (payload.size() < size_t(1) + name_len + 1 + pass_len) return false;

    username.assign(payload.begin() + 1, payload.begin() + 1 + name_len);
    password.assign(payload.begin() + 1 + name_len + 1, payload.begin() + 1 + name_len + 1 + pass_len);
    return true;
}

bool AuthService::IsUsernameValid(const std::string &username)
{
    if (username.size() < 3 || username.size() > 16) return false;
    for (char c : username)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            return false;
    return true;
}
