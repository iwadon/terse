# 統合テストバイナリ
add_executable(terse_unit_test
	unit/terse_unit_test.c
	unit/terse_ambiguous_width_test.c
	unit/terse_char_width_test.c
	unit/terse_clipboard_test.c
	unit/terse_close_test.c
	unit/terse_cursor_shape_test.c
	unit/terse_encode_utf8_test.c
	unit/terse_event_helpers_test.c
	unit/terse_function_keys_test.c
	unit/terse_get_cursor_position_test.c
	unit/terse_get_size_test.c
	unit/terse_hyperlink_test.c
	unit/terse_image_test.c
	unit/terse_keyboard_features_test.c
	unit/terse_mouse_test.c
	unit/terse_nav_keys_verify.c
	unit/terse_notification_test.c
	unit/terse_open_test.c
	unit/terse_options_test.c
	unit/terse_output_test.c
	unit/terse_paste_test.c
	unit/terse_read_event_ctrl_test.c
	unit/terse_read_event_test.c
	unit/terse_state_stack_test.c
	unit/terse_style_test.c
	unit/terse_title_test.c
	unit/terse_write_text_test.c
)
target_include_directories(terse_unit_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(terse_unit_test PRIVATE terse attest)
add_test(NAME c.terse_unit_test COMMAND terse_unit_test)
