# 統合テストバイナリ
add_executable(terse_unit_test
	unit/terse_unit_test.c
	unit/terse_ambiguous_width_test.c
	unit/terse_basic4_test.c
	unit/terse_buffer_test.c
	unit/terse_char_width_test.c
	unit/terse_clipboard_test.c
	unit/terse_close_test.c
	unit/terse_cursor_shape_test.c
	unit/terse_encode_utf8_test.c
	unit/terse_event_helpers_test.c
	unit/terse_function_keys_test.c
	unit/terse_get_cursor_position_test.c
	unit/terse_get_size_test.c
	unit/terse_golden_test.c
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
	unit/terse_posix_ext_test.c
	unit/terse_pty_test.c
	unit/terse_read_event_ctrl_test.c
	unit/terse_read_event_test.c
	unit/terse_require_test.c
	unit/terse_state_stack_test.c
	unit/terse_style_test.c
	unit/terse_title_test.c
	unit/terse_write_text_test.c
)
target_include_directories(terse_unit_test PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../src
	${CMAKE_CURRENT_SOURCE_DIR}/../src/term
	${CMAKE_CURRENT_SOURCE_DIR}/../src/core)
target_link_libraries(terse_unit_test PRIVATE terse attest)
# テストはC11でコンパイルする（terse本体はC99）
set_target_properties(terse_unit_test PROPERTIES
	C_STANDARD 11
	C_STANDARD_REQUIRED ON
	C_EXTENSIONS OFF
)
# POSIX拡張API (posix_openpt, sigjmp_buf等) を有効にする
if(UNIX)
	target_compile_definitions(terse_unit_test PRIVATE _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)
endif()
# ゴールデンテスト基盤 (Phase 6): ソースツリー内のゴールデン格納先を渡す。
# 通常実行は読み取り専用比較、UPDATE_GOLDEN=1 のときのみ上書きする。
target_compile_definitions(terse_unit_test PRIVATE
	TERSE_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
add_test(NAME terse_unit_test COMMAND terse_unit_test)
