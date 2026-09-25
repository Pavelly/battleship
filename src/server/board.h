#pragma once

#include "ship.h"
#include "protocol.h"
#include <vector>
#include <optional>

enum class CellState : uint8_t {
    EMPTY = 0,
    SHIP = 0x01,
    HIT = 0x02,
    MISS = 0x03
};

enum class PlacementResult : uint8_t {
    OK = 0,
    OUT_OF_BOUNDS = 0x01,
    OVERLAPS = 0x02,
    TOUCHES = 0x03,
    INVALID_SIZE = 0x04,
    TOO_MANY_SHIPS = 0x05
};

class Board {
public:
    static constexpr uint8_t SIZE = 10;

    Board() {}

    PlacementResult PlaceShip(uint8_t row, uint8_t col, uint8_t size, Orientation orientation);
    bool IsPlacementComplete() const;
    ShotResult Shoot(uint8_t row, uint8_t col);
    bool AllShipsSunk() const;
    CellState GetCellState(uint8_t row, uint8_t col) const;
    size_t GetShipCount() const { return ships_.size(); }
    void Reset();
    void Print(bool show_ships = true) const;
private:
    std::vector<Ship> ships_;

    PlacementResult CanPlaceShip(uint8_t row, uint8_t col, uint8_t size, Orientation orientation) const;
    bool IsWithinBounds(uint8_t row, uint8_t col, uint8_t size, Orientation orientation) const;
    bool IsCellNearShip(uint8_t row, uint8_t col) const;
    int FindShipAt(uint8_t row, uint8_t col) const;
};