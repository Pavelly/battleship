set(SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

add_library(battleship_common STATIC
    common/protocol.cpp
)
target_include_directories(battleship_common PUBLIC ${SRC_ROOT})
set_strict_warnings(battleship_common)

add_library(battleship_game STATIC
    game/ship.cpp
    game/board.cpp
)
target_include_directories(battleship_game PUBLIC ${SRC_ROOT})
target_link_libraries(battleship_game PUBLIC battleship_common)
set_strict_warnings(battleship_game)

add_executable(battleship_server
    net/session.cpp
    server/main.cpp
    server/server.cpp
    server/lobby.cpp
    server/game.cpp
)
target_include_directories(battleship_server PRIVATE ${SRC_ROOT})
target_link_libraries(battleship_server PRIVATE battleship_game)
set_strict_warnings(battleship_server)

if(WIN32)
    target_link_libraries(battleship_server PRIVATE ws2_32)
endif()