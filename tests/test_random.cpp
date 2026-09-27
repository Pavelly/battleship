#include "game/random_fleet.h"
#include <cassert>
#include <iostream>

int main() {
    // 1. Детерминированность при одинаковом зерне
    {
        Board b1, b2;
        std::mt19937 r1(42), r2(42);
        const auto s1 = PlaceRandomFleet(b1, r1);
        const auto s2 = PlaceRandomFleet(b2, r2);
        assert(s1.size() == 10);
        assert(s1 == s2);
        std::cout << "Determinism with fixed seed: OK\n";
    }

    // 2. Массовая проверка валидности: 500 случайных флотов
    {
        std::mt19937 rng(std::random_device{}());
        for (int i = 0; i < 500; ++i) {
            Board board;
            const auto specs = PlaceRandomFleet(board, rng);

            assert(specs.size() == 10);                 // весь флот расставлен
            assert(board.IsPlacementComplete());

            // Независимая перепроверка: спецификации образуют валидный флот
            Board checker;
            for (const auto& s : specs) {
                assert(checker.PlaceShip(s.row, s.col, s.size, s.orientation)
                       == PlacementResult::OK);
            }
        }
        std::cout << "500 random fleets valid: OK\n";
    }

    // 3. Состав флота: 1x4, 2x3, 3x2, 4x1
    {
        Board board;
        std::mt19937 rng(7);
        const auto specs = PlaceRandomFleet(board, rng);
        int counts[5] = {0, 0, 0, 0, 0};
        for (const auto& s : specs) counts[s.size]++;
        assert(counts[1] == 4 && counts[2] == 3 && counts[3] == 2 && counts[4] == 1);
        std::cout << "Fleet composition: OK\n";
    }

    std::cout << "All random-fleet tests passed!\n";
    return 0;
}