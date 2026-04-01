#ifndef TERSE_TEST_H_INCLUDED
#define TERSE_TEST_H_INCLUDED

#include "terse.h"

#ifdef TERSE_ENABLE_TEST_MODE

#ifdef __cplusplus
extern "C" {
#endif

typedef enum terse_call_type {
	TERSE_CALL_WRITE_TEXT = 0,
	TERSE_CALL_MOVE_TO,
	TERSE_CALL_CLEAR_SCREEN,
	TERSE_CALL_CLEAR_LINE,
	TERSE_CALL_SHOW_CURSOR,
	TERSE_CALL_SET_STYLE,
	TERSE_CALL_ENABLE_MOUSE,
	TERSE_CALL_DISABLE_MOUSE,
	TERSE_CALL_SET_TITLE,
	TERSE_CALL_FLUSH,
} terse_call_type_t;

typedef struct terse_call_record {
	terse_call_type_t type;
	union {
		struct {
			char text[256];
		} write_text;
		struct {
			int row;
			int col;
		} move_to;
		struct {
			terse_clear_mode_t mode;
		} clear_screen;
		struct {
			terse_clear_mode_t mode;
		} clear_line;
		struct {
			int visible;
		} show_cursor;
		struct {
			terse_style_t style;
		} set_style;
		struct {
			terse_mouse_mode_t mode;
		} enable_mouse;
		struct {
			char title[256];
		} set_title;
	} data;
} terse_call_record_t;

// Recording API
int terse_test_start_recording(terse_handle_t handle);
int terse_test_stop_recording(terse_handle_t handle);
const terse_call_record_t *terse_test_get_calls(terse_handle_t handle, int *out_count);
int terse_test_clear_calls(terse_handle_t handle);

// Mocking API
int terse_test_mock_capabilities(terse_handle_t handle, const terse_capabilities_t *caps);
int terse_test_mock_size(terse_handle_t handle, int rows, int cols);
int terse_test_mock_events(terse_handle_t handle, const terse_event_t *events, int count);
int terse_test_reset_mocks(terse_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // TERSE_ENABLE_TEST_MODE

#endif // TERSE_TEST_H_INCLUDED
