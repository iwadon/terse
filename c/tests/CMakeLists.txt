add_executable(terse_open_test unit/terse_open_test.c)
target_link_libraries(terse_open_test PRIVATE terse)
add_test(NAME c.terse_open_test COMMAND terse_open_test)
