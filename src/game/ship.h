#pragma once

#include <cstdint>
#include <vector>
#include <utility>

enum class Orientation : uint8_t {
    HORIZONTAL = 0,
    VERTICAL = 0x01
};

struct Coord {
    uint8_t row;
    uint8_t col;

    bool operator==(const Coord& other) const {
        return row == other.row && col == other.col;
    }
};

class Ship {
public:
    Ship(uint8_t row, uint8_t col, uint8_t size, Orientation orientation);

    std::vector<Coord> GetCells() const;
    bool Occupies(uint8_t row, uint8_t col) const;
    bool TryHit(uint8_t row, uint8_t col);
    bool IsSunk() const;
    bool WasShotAt(uint8_t row, uint8_t col) const;

    uint8_t GetRow() const { return row_; }
    uint8_t GetCol() const { return col_; }
    uint8_t GetSize() const { return size_; }
    Orientation GetOrientation() const { return orientation_; }
private:
    uint8_t row_;
    uint8_t col_;
    uint8_t size_;
    Orientation orientation_;
    std::vector<bool> hits_;
};