add_executable(test_model test_model.cpp)
target_link_libraries(test_model PRIVATE battleship_game)
set_strict_warnings(test_model)
add_test(NAME model_tests COMMAND test_model)

add_executable(test_db test_db.cpp)
target_link_libraries(test_db PRIVATE battleship_db)
set_strict_warnings(test_db)
add_test(NAME db_tests COMMAND test_db)

add_executable(test_crypto test_crypto.cpp)
target_link_libraries(test_crypto PRIVATE battleship_common)
set_strict_warnings(test_crypto)
add_test(NAME crypto_tests COMMAND test_crypto)

add_executable(test_auth test_auth.cpp)
target_link_libraries(test_auth PRIVATE battleship_db)
set_strict_warnings(test_auth)
add_test(NAME auth_tests COMMAND test_auth)

add_executable(test_online test_online.cpp)
target_link_libraries(test_online PRIVATE battleship_net)
set_strict_warnings(test_online)
add_test(NAME online_tests COMMAND test_online)

add_executable(test_random test_random.cpp)
target_link_libraries(test_random PRIVATE battleship_game)
set_strict_warnings(test_random)
add_test(NAME random_tests COMMAND test_random)

add_executable(test_outline test_outline.cpp)
target_link_libraries(test_outline PRIVATE battleship_game)
set_strict_warnings(test_outline)
add_test(NAME outline_tests COMMAND test_outline)

add_executable(test_timer test_timer.cpp)
target_link_libraries(test_timer PRIVATE battleship_match)
set_strict_warnings(test_timer)
add_test(NAME timer_tests COMMAND test_timer)

add_executable(test_rooms test_rooms.cpp)
target_link_libraries(test_rooms PRIVATE battleship_match)
set_strict_warnings(test_rooms)
add_test(NAME room_tests COMMAND test_rooms)

add_executable(test_rematch test_rematch.cpp)
target_link_libraries(test_rematch PRIVATE battleship_match)
set_strict_warnings(test_rematch)
add_test(NAME rematch_tests COMMAND test_rematch)

add_executable(test_logger test_logger.cpp)
target_link_libraries(test_logger PRIVATE battleship_common)
set_strict_warnings(test_logger)
add_test(NAME logger_tests COMMAND test_logger)

add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE battleship_common)
set_strict_warnings(test_config)
add_test(NAME config_tests COMMAND test_config)

add_executable(test_history test_history.cpp)
target_link_libraries(test_history PRIVATE battleship_db)
set_strict_warnings(test_history)
add_test(NAME history_tests COMMAND test_history)