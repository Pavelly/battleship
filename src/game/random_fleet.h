#pragma once

#include "game/board.h"
#include <cstdint>
#include <random>
#include <vector>

struct ShipSpec {
    uint8_t row;
    uint8_t col;
    uint8_t size;
    Orientation orientation;

    bool operator==(const ShipSpec& o) const {
        return row == o.row && col == o.col && size == o.size && orientation == o.orientation;
    }
};

std::vector<ShipSpec> PlaceRandomFleet(Board& board, std::mt19937& rng);