#include "game/board.h"
#include <cassert>
#include <iostream>

int main() {
    // 1. Промахи запоминаются, повторный выстрел по воде отклоняется
    {
        Board board;
        board.PlaceShip(0, 0, 1, Orientation::HORIZONTAL);
        assert(board.Shoot(5, 5) == ShotResult::MISS);
        assert(board.GetCellState(5, 5) == CellState::MISS);
        assert(board.Shoot(5, 5) == ShotResult::ALREADY_SHOT);
        std::cout << "Miss tracking: OK\n";
    }

    // 2. Обводка углового корабля 1х4: ровно 6 клеток
    {
        Board board;
        board.PlaceShip(0, 0, 4, Orientation::HORIZONTAL);
        for (uint8_t c = 0; c < 4; ++c)
            board.Shoot(0, c);
        const auto outlined = board.MarkOutlineAround(0, 3);
        assert(outlined.size() == 6);
        assert(board.GetCellState(0, 4) == CellState::MISS);
        assert(board.GetCellState(1, 4) == CellState::MISS);
        assert(board.GetCellState(0, 0) == CellState::HIT);
        std::cout << "Corner outline(6 cells): OK\n";
    }

    // 3. Обводка корабля 1х3 в центре: ровно 12 клеток; повторный вызов пуст
    {
        Board board;
        board.PlaceShip(4, 4, 3, Orientation::HORIZONTAL);
        for (uint8_t c = 4; c < 7; ++c)
            board.Shoot(4, c);
        const auto outlined = board.MarkOutlineAround(4, 5);
        assert(outlined.size() == 12);
        assert(board.MarkOutlineAround(4, 5).empty());
        std::cout << "Middle outline (12 cells): OK\n";
    }

    // 4. По обведенным клеткам стрелять больше нельзя
    {
        Board board;
        board.PlaceShip(0, 0, 1, Orientation::HORIZONTAL);
        board.Shoot(0, 0);
        const auto outlined = board.MarkOutlineAround(0, 0);
        assert(outlined.size() == 3);
        assert(board.Shoot(outlined[0].row, outlined[0].col) == ShotResult::ALREADY_SHOT);
        std::cout << "Outlined cells protected: OK\n";
    }

    std::cout << "All outline tests passed!\n";
    return 0;
}