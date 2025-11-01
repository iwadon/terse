# 統合テストバイナリ
add_executable(terse_unit_test
	unit/terse_unit_test.c
	unit/terse_clipboard_test.c
	unit/terse_close_test.c
	unit/terse_cursor_shape_test.c
	unit/terse_get_cursor_position_test.c
	unit/terse_get_size_test.c
	unit/terse_hyperlink_test.c
	unit/terse_image_test.c
	unit/terse_keyboard_features_test.c
	unit/terse_mouse_test.c
	unit/terse_notification_test.c
	unit/terse_open_test.c
	unit/terse_options_test.c
	unit/terse_output_test.c
	unit/terse_paste_test.c
	unit/terse_read_event_ctrl_test.c
	unit/terse_read_event_test.c
	unit/terse_style_test.c
	unit/terse_title_test.c
	unit/terse_write_text_test.c
)
target_link_libraries(terse_unit_test PRIVATE terse attest)
add_test(NAME c.terse_unit_test COMMAND terse_unit_test)
