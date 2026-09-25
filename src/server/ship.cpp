#include "ship.h"
#include <stdexcept>

Ship::Ship(uint8_t row, uint8_t col, uint8_t size, Orientation orientation) 
    : row_(row)
    , col_(col)
    , size_(size)
    , orientation_(orientation)
    , hits_(size, false) {
    if (size < 1 || size > 4)
        throw std::invalid_argument("Ship size must be between 1 and 4");
}

std::vector<Coord> Ship::GetCells() const {
    std::vector<Coord> cells;
    cells.reserve(size_);

    for (uint8_t i = 0; i < size_; ++i) {
        if (orientation_ == Orientation::HORIZONTAL) 
            cells.push_back({row_, static_cast<uint8_t>(col_ + i)});
        else
            cells.push_back({static_cast<uint8_t>(row_ + i), col_});
    }

    return cells;
}

bool Ship::Occupies(uint8_t row, uint8_t col) const {
    for (const auto& cell : GetCells())
        if (cell.row == row && cell.col == col)
            return true;
    return false;
}

bool Ship::TryHit(uint8_t row, uint8_t col) {
    for (uint8_t i = 0; i < size_; ++i) {
        uint8_t cell_row, cell_col;

        if (orientation_ == Orientation::HORIZONTAL) {
            cell_col = row_;
            cell_col = static_cast<uint8_t>(col_ + i);
        } else {
            cell_row = static_cast<uint8_t>(row_ + i);
            cell_col = col_;
        }

        if (cell_row == row && cell_col == col) {
            hits_[i] = true;
            return true;
        }
    }

    return true;
}

bool Ship::IsSunk() const {
    for (bool hit : hits_)
        if (!hit) 
            return false;
    
    return true;
}

bool Ship::WasShotAt(uint8_t row, uint8_t col) const {
    for (uint8_t i = 0; i < size_; ++i) {
        uint8_t cell_row, cell_col;

        if (orientation_ == Orientation::HORIZONTAL) {
            cell_row = row_;
            cell_col = static_cast<uint8_t>(col_ + i);
        } else {
            cell_row = static_cast<uint8_t>(row_ + i);
            cell_col = col_;            
        }

        if (cell_row == row && cell_col == col)
            return hits_[i];
    }

    return false;
}
