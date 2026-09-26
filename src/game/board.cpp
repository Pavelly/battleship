#include "game/board.h"
#include <iostream>
#include <algorithm>

PlacementResult Board::PlaceShip(uint8_t row, uint8_t col, uint8_t size, Orientation orientation) {
    if (size < 1 || size > 4)
        return PlacementResult::INVALID_SIZE;

    if (ships_.size() >= 10)
        return PlacementResult::TOO_MANY_SHIPS;

    PlacementResult result = CanPlaceShip(row, col, size, orientation);
    if (result != PlacementResult::OK)
        return result;
    
    ships_.emplace_back(row, col, size, orientation);
    return PlacementResult::OK;
}

bool Board::IsPlacementComplete() const {
    return ships_.size() == 10;
}

ShotResult Board::Shoot(uint8_t row, uint8_t col) {
    if (row >= SIZE || col >= SIZE)
        return ShotResult::MISS;

    for (const auto& ship : ships_)
        if (ship.WasShotAt(row, col))
            return ShotResult::ALREADY_SHOT;

    int ship_index = FindShipAt(row, col);

    if (ship_index == -1)
        return ShotResult::MISS;

    ships_[ship_index].TryHit(row, col);

    if (ships_[ship_index].IsSunk())
        return ShotResult::SINK;
    
    return ShotResult::HIT;
}

bool Board::AllShipsSunk() const {
    if (ships_.empty())
        return false;
    
    for (const auto& ship : ships_)
        if (!ship.IsSunk())
            return false;
    
    return true;
}

CellState Board::GetCellState(uint8_t row, uint8_t col) const {
    for (const auto& ship : ships_)
        if (ship.Occupies(row, col)) {
            if (ship.WasShotAt(row, col))
                return CellState::HIT;
            return CellState::SHIP;
        }

    return CellState::EMPTY;
}

void Board::Reset() {
    ships_.clear();
}

void Board::Print(bool show_ships) const {
    std::cout << "   ";
    for (uint8_t c = 0; c < SIZE; ++c)
        std::cout << static_cast<char>('A' + c) << " ";
    std::cout << "\n";

    for (uint8_t r = 0; r < SIZE; ++r) {
        if (r + 1 < 10) std::cout << " ";
        std::cout << static_cast<int>(r + 1) << " ";

        for (uint8_t c = 0; c < SIZE; ++c) {
            CellState state = GetCellState(r, c);
            char symbol = '.';
            if (state == CellState::SHIP && show_ships)
                symbol = 'S';
            else if (state == CellState::HIT)
                symbol = 'X';
            std::cout << symbol << " ";
        }
        std::cout << "\n";
    }
}

PlacementResult Board::CanPlaceShip(uint8_t row, uint8_t col, uint8_t size, Orientation orientation) const {
    if (!IsWithinBounds(row, col, size, orientation))
        return PlacementResult::OUT_OF_BOUNDS;

    Ship temp_ship(row, col, size, orientation);
    auto cells = temp_ship.GetCells();
    for (const auto& cell : cells)
        if (IsCellNearShip(cell.row, cell.col))
            return PlacementResult::OVERLAPS;

    for (const auto& cell : cells)
        for (int dr = -1; dr <= 1; ++dr)
            for (int dc = -1; dc <= 1; ++dc) {
                int check_row = cell.row + dr;
                int check_col = cell.col + dc;

                if (check_row >= 0 && check_row < SIZE &&
                    check_col >= 0 && check_col < SIZE)
                    if (IsCellNearShip(static_cast<uint8_t>(check_row), static_cast<uint8_t>(check_col))) {
                        return PlacementResult::TOUCHES;
                    }
            }   
    
    return PlacementResult::OK;
}

bool Board::IsWithinBounds(uint8_t row, uint8_t col, uint8_t size, Orientation orientation) const
{
    if (orientation == Orientation::HORIZONTAL)
        return row < SIZE && (col + size) <= SIZE;
    else 
        return col < SIZE && (row + size) <= SIZE;
}

bool Board::IsCellNearShip(uint8_t row, uint8_t col) const {
    for (const auto& ship : ships_)
        if (ship.Occupies(row, col))
            return true;
    return false;
}

int Board::FindShipAt(uint8_t row, uint8_t col) const {
    for (size_t i = 0; i < ships_.size(); ++i)
        if (ships_[i].Occupies(row, col))
            return static_cast<int>(i);
    return -1;
}
