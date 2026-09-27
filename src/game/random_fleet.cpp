#include "random_fleet.h"
#include <array>

std::vector<ShipSpec> PlaceRandomFleet(Board &board, std::mt19937 &rng) {
    static const std::array<uint8_t, 10> kFleet = {4, 3, 3, 2, 2, 2, 1, 1, 1, 1};

    std::uniform_int_distribution<int> coord(0, Board::SIZE - 1);
    std::uniform_int_distribution<int> orient(0, 1);

    std::vector<ShipSpec> placed;
    placed.reserve(kFleet.size());

    for (int restart = 0; restart < 100; ++restart) {
        board.Reset();
        placed.clear();

        bool fleet_ok = true;
        for (uint8_t size : kFleet) {
            bool ship_placed = false;

            for (int attempt = 0; attempt < 200; ++attempt) {
                const uint8_t row = static_cast<uint8_t>(coord(rng));
                const uint8_t col = static_cast<uint8_t>(coord(rng));
                const Orientation o = (orient(rng) == 0)
                    ? Orientation::HORIZONTAL
                    : Orientation::VERTICAL;

                if (board.PlaceShip(row, col, size, o) == PlacementResult::OK) {
                    placed.push_back(ShipSpec{row, col, size, o});
                    ship_placed = true;
                    break;
                }
            }

            if (!ship_placed) {
                fleet_ok = false;
                break;
            }
        }
        if (fleet_ok) return placed;
    }
    board.Reset();
    return {};
}