cmake_minimum_required(VERSION 3.15)
project(tcp_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(battleship_server
    src/server/main.cpp
    src/server/server.cpp
    src/server/session.cpp
    src/server/lobby.cpp
    src/server/game.cpp
    src/server/protocol.cpp
    src/server/ship.cpp
    src/server/board.cpp
)

target_include_directories(battleship PRIVATE src)

add_executable(test_model
    src/test_model.cpp
    src/server/ship.cpp
    src/server/board.cpp
    src/server/protocol.cpp
)

target_include_directories(test_model PRIVATE src)

if(MSVC)
    target_compile_options(battleship PRIVATE /W4 /WX)
    target_compile_options(test_model PRIVATE /W4 /WX)
else()
    target_compile_options(battleship PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(test_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(WIN32)
    target_link_libraries(battleship PRIVATE ws2_32)
endif()