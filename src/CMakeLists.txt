set(SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

add_library(battleship_common STATIC
    common/protocol.cpp
    common/sha256.cpp
    common/password.cpp
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

add_library(sqlite3 STATIC ${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_SOURCE_DIR}/third_party/sqlite)

add_library(battleship_db STATIC 
    db/database.cpp
    db/auth_service.cpp
)
target_include_directories(battleship_db PUBLIC ${SRC_ROOT})
target_link_libraries(battleship_db PUBLIC sqlite3 battleship_common)
set_strict_warnings(battleship_db)

target_include_directories(battleship_server PRIVATE ${SRC_ROOT})
target_link_libraries(battleship_server PRIVATE battleship_game battleship_db)
set_strict_warnings(battleship_server)

if(WIN32)
    target_link_libraries(battleship_server PRIVATE ws2_32)
endif()