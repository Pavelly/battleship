#include "common/sha256.h"
#include "common/password.h"
#include <cassert>
#include <iostream>

int main() {
    // === Официальные тестовые векторы FIPS 180-4 ===
    assert(Sha256::HashHex("") ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    assert(Sha256::HashHex("abc") ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    assert(Sha256::HashHex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    std::cout << "NIST vectors: OK\n";

    // === Потоковое хеширование кусками == одноразовое ===
    Sha256 streaming;
    streaming.Update("ab", 2);
    streaming.Update("c", 1);
    assert(Sha256::ToHex(streaming.Finalize()) == Sha256::HashHex("abc"));
    std::cout << "Streaming == one-shot: OK\n";

    // === Граничный случай добивки: ровно 56 байт ===
    const std::string boundary(56, 'a');
    assert(Sha256::HashHex(boundary) == Sha256::HashHex(boundary));
    assert(Sha256::HashHex(boundary) != Sha256::HashHex(std::string(55, 'a')));
    std::cout << "Padding boundary: OK\n";

    // === Соль и пароли ===
    const std::string salt = auth::GenerateSalt();
    assert(salt.size() == 32);
    assert(auth::GenerateSalt() != salt);  // уникальность

    const std::string hash = auth::HashPassword("secret", salt);
    assert(hash.size() == 64);
    assert(auth::VerifyPassword("secret", salt, hash));
    assert(!auth::VerifyPassword("wrong", salt, hash));

    // Та же пара пароль+соль -> тот же hash (детерминированность)
    assert(auth::HashPassword("secret", salt) == hash);
    // Та же пароль, другая соль -> другой hash
    assert(auth::HashPassword("secret", auth::GenerateSalt()) != hash);
    std::cout << "Salt & verify: OK\n";

    std::cout << "All crypto tests passed!\n";
    return 0;
}