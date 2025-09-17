add_executable(terse_open_test unit/terse_open_test.c)
target_link_libraries(terse_open_test PRIVATE terse)
add_test(NAME c.terse_open_test COMMAND terse_open_test)

add_executable(terse_write_text_test unit/terse_write_text_test.c)
target_link_libraries(terse_write_text_test PRIVATE terse)
add_test(NAME c.terse_write_text_test COMMAND terse_write_text_test)

add_executable(terse_output_test unit/terse_output_test.c)
target_link_libraries(terse_output_test PRIVATE terse)
add_test(NAME c.terse_output_test COMMAND terse_output_test)

add_executable(terse_read_event_test unit/terse_read_event_test.c)
target_link_libraries(terse_read_event_test PRIVATE terse)
add_test(NAME c.terse_read_event_test COMMAND terse_read_event_test)

add_executable(terse_get_size_test unit/terse_get_size_test.c)
target_link_libraries(terse_get_size_test PRIVATE terse)
add_test(NAME c.terse_get_size_test COMMAND terse_get_size_test)

add_executable(terse_options_test unit/terse_options_test.c)
target_link_libraries(terse_options_test PRIVATE terse)
add_test(NAME c.terse_options_test COMMAND terse_options_test)

add_executable(terse_close_test unit/terse_close_test.c)
target_link_libraries(terse_close_test PRIVATE terse)
add_test(NAME c.terse_close_test COMMAND terse_close_test)
