#include "server/board.h"
#include <iostream>
#include <cassert>

void TestShipPlacement() {
    std::cout << "=== Test: Ship Placement ===\n";

    Board board;

    auto result = board.PlaceShip(0, 0, 4, Orientation::HORIZONTAL);
    assert(result == PlacementResult::OK);
    std::cout << "Placed 4-cell ship: OK\n";
  
    result = board.PlaceShip(2, 0, 3, Orientation::HORIZONTAL);
    assert(result == PlacementResult::OK);
    result = board.PlaceShip(4, 0, 3, Orientation::HORIZONTAL);
    assert(result == PlacementResult::OK);

    result = board.PlaceShip(6, 0, 2, Orientation::HORIZONTAL);
    assert(result == PlacementResult::OK);
    result = board.PlaceShip(8, 0, 2, Orientation::HORIZONTAL);
    assert(result == PlacementResult::OK);
    result = board.PlaceShip(6, 3, 2, Orientation::HORIZONTAL);
    assert(result == PlacementResult::OK);

    result = board.PlaceShip(0, 5, 1, Orientation::HORIZONTAL);
    assert(result == PlacementResult::OK);
    result = board.PlaceShip(2, 4, 1, Orientation::HORIZONTAL);
    assert(result == PlacementResult::OK);
    result = board.PlaceShip(4, 4, 1, Orientation::HORIZONTAL);
    assert(result == PlacementResult::OK);
    result = board.PlaceShip(8, 3, 1, Orientation::HORIZONTAL);
    assert(result == PlacementResult::OK);

    assert(board.IsPlacementComplete());
    std::cout << "All 10 ships placed successfully!\n\n";

    board.Print();
    std::cout << std::endl;
}

void TestInvalidPlacement() {
    std::cout << "=== Test: Ivalid Placement ===\n";    

    Board board;

    auto result = board.PlaceShip(0, 8, 4, Orientation::HORIZONTAL);
    assert(result == PlacementResult::OUT_OF_BOUNDS);
    std::cout << "Out of bounds detected: OK\n";

    board.PlaceShip(0, 0, 4, Orientation::HORIZONTAL);

    result = board.PlaceShip(0, 2, 3, Orientation::HORIZONTAL);
    assert(result == PlacementResult::OVERLAPS);
    std::cout << "Overlap detected: OK\n";

    result = board.PlaceShip(1, 0, 2, Orientation::HORIZONTAL);
    assert(result == PlacementResult::TOUCHES);
    std::cout << "Touch detected: OK\n";

    std::cout << std::endl;
}

void TestShooting() {
    std::cout << "=== Test: Shooting ===\n";

    Board board;
    board.PlaceShip(0, 0, 4, Orientation::HORIZONTAL);

    auto result = board.Shoot(5, 5);
    assert(result == ShotResult::MISS);
    std::cout << "Miss: OK\n";

    result = board.Shoot(0, 0);
    assert(result == ShotResult::HIT);
    std::cout << "Hit: OK\n";
    
    result = board.Shoot(0, 0);
    assert(result == ShotResult::ALREADY_SHOT);
    std::cout << "Already shot: OK\n";

    result = board.Shoot(0, 1);
    assert(result == ShotResult::HIT);
    result = board.Shoot(0, 2);
    assert(result == ShotResult::HIT);
    result = board.Shoot(0, 3);
    assert(result == ShotResult::SINK);
    std::cout << "Ship sunk: OK\n";

    std::cout << std::endl;
}

void TestWinCondition() {
    std::cout << "=== Test: Win Condition ===\n";

    Board board;

    board.PlaceShip(0, 0, 1, Orientation::HORIZONTAL);

    assert(!board.AllShipsSunk());
    std::cout << "Not all ships sunk: OK\n";

    board.Shoot(0, 0);

    assert(board.AllShipsSunk());
    std::cout << "All ships sunk (win): OK\n";

    std::cout << std::endl;
}

int main() {
    std::cout << "Running game model tests...\n\n";

    TestShipPlacement();
    TestInvalidPlacement();
    TestShooting();
    TestWinCondition();

    std::cout << "All tests passed\n";
    return 0;
}