#include "net/online_registry.h"
#include "net/session.h"
#include <cassert>
#include <iostream>

int main() {
    OnlineRegistry reg;

    auto s1 = std::make_shared<Session>(INVALID_SOCK);
    auto s2 = std::make_shared<Session>(INVALID_SOCK);

    // Первый вход свободен
    assert(reg.TryAcquire(1, s1));

    // Вторая сессия того же аккаунта блокируется
    assert(!reg.TryAcquire(1, s2));

    // Другой аккаунт не мешает
    assert(reg.TryAcquire(2, s2));

    // Чужая сессия не может освободить слот
    reg.Release(1, s2);
    assert(!reg.TryAcquire(1, s2));

    // Сессия умерла (weak_ptr протух) — слот перезаписывается
    s1.reset();
    assert(reg.TryAcquire(1, s2));

    // Владелец освобождает свой слот
    reg.Release(1, s2);
    assert(reg.TryAcquire(1, s2));
    reg.Release(1, s2);

    std::cout << "All online-registry tests passed!\n";
    return 0;
}