add_executable(test_model
    test_model.cpp
)
target_link_libraries(test_model PRIVATE battleship_game)
set_strict_warnings(test_model)

add_test(NAME model_tests COMMAND test_model)