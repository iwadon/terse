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

add_executable(terse_read_event_ctrl_test unit/terse_read_event_ctrl_test.c)
target_link_libraries(terse_read_event_ctrl_test PRIVATE terse)
add_test(NAME c.terse_read_event_ctrl_test COMMAND terse_read_event_ctrl_test)

add_executable(terse_mouse_test unit/terse_mouse_test.c)
target_link_libraries(terse_mouse_test PRIVATE terse)
add_test(NAME c.terse_mouse_test COMMAND terse_mouse_test)

add_executable(terse_paste_test unit/terse_paste_test.c)
target_link_libraries(terse_paste_test PRIVATE terse)
add_test(NAME c.terse_paste_test COMMAND terse_paste_test)

add_executable(terse_title_test unit/terse_title_test.c)
target_link_libraries(terse_title_test PRIVATE terse)
add_test(NAME c.terse_title_test COMMAND terse_title_test)

add_executable(terse_cursor_shape_test unit/terse_cursor_shape_test.c)
target_link_libraries(terse_cursor_shape_test PRIVATE terse)
add_test(NAME c.terse_cursor_shape_test COMMAND terse_cursor_shape_test)

add_executable(terse_clipboard_test unit/terse_clipboard_test.c)
target_link_libraries(terse_clipboard_test PRIVATE terse)
add_test(NAME c.terse_clipboard_test COMMAND terse_clipboard_test)

add_executable(terse_image_test unit/terse_image_test.c)
target_link_libraries(terse_image_test PRIVATE terse)
add_test(NAME c.terse_image_test COMMAND terse_image_test)

add_executable(terse_notification_test unit/terse_notification_test.c)
target_link_libraries(terse_notification_test PRIVATE terse)
add_test(NAME c.terse_notification_test COMMAND terse_notification_test)

add_executable(terse_style_test unit/terse_style_test.c)
target_link_libraries(terse_style_test PRIVATE terse)
add_test(NAME c.terse_style_test COMMAND terse_style_test)

add_executable(terse_hyperlink_test unit/terse_hyperlink_test.c)
target_link_libraries(terse_hyperlink_test PRIVATE terse)
add_test(NAME c.terse_hyperlink_test COMMAND terse_hyperlink_test)

add_executable(terse_keyboard_features_test unit/terse_keyboard_features_test.c)
target_link_libraries(terse_keyboard_features_test PRIVATE terse)
add_test(NAME c.terse_keyboard_features_test COMMAND terse_keyboard_features_test)
