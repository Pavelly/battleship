add_executable(test_model
    test_model.cpp
)
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