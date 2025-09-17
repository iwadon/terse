add_executable(terse_open_test unit/terse_open_test.c)
target_link_libraries(terse_open_test PRIVATE terse)
add_test(NAME c.terse_open_test COMMAND terse_open_test)

add_executable(terse_write_text_test unit/terse_write_text_test.c)
target_link_libraries(terse_write_text_test PRIVATE terse)
add_test(NAME c.terse_write_text_test COMMAND terse_write_text_test)

add_executable(terse_output_test unit/terse_output_test.c)
target_link_libraries(terse_output_test PRIVATE terse)
add_test(NAME c.terse_output_test COMMAND terse_output_test)
