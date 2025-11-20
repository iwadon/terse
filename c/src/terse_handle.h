#ifndef TERSE_HANDLE_H
#define TERSE_HANDLE_H

#include "terse.h"
#include "terse_test.h"

#ifndef TERSE_USE_SYSTEM_ICONV
#define TERSE_USE_SYSTEM_ICONV 1
#endif

#if TERSE_USE_SYSTEM_ICONV
#include <iconv.h>
#else
#include "mini_iconv.h"
#endif

/* State history stack depth macro.  Small for sanity, yet enough for
 * typical nested UI layers.
 */
#define TERSE_STATE_STACK_MAX 8

typedef enum terse_codec_kind {
	TERSE_CODEC_UNKNOWN = 0,
	TERSE_CODEC_UTF8,
	TERSE_CODEC_SHIFT_JIS
} terse_codec_kind_t;

/* Internal handle structure definition.
 * This is shared across implementation modules (terse.c, terse_output.c, etc.)
 * but NOT exposed to public API.
 */
struct terse_handle {
	terse_profile_t requested_profile;
	terse_capabilities_t capabilities;
	terse_capabilities_t detected_capabilities;
	terse_options_t options;
	terse_codec_kind_t codec_kind;
	iconv_t codec_to_utf8;
	iconv_t utf8_to_codec;
	terse_size_t size;
	int cursor_visible;
	int cursor_row;
	int cursor_col;
	int cursor_known;
	terse_style_t style;
	terse_style_t effective_style;
	int style_known;
	terse_mouse_mode_t mouse_mode;
	int mouse_enabled;
	terse_mouse_button_t mouse_button;
	int paste_enabled;
	terse_error_t last_error;
	unsigned int runtime_enabled;
	unsigned int runtime_disabled;
	unsigned int keyboard_supported;
	unsigned int keyboard_enabled;
	// State history
	terse_state_t state_stack[TERSE_STATE_STACK_MAX];
	int state_stack_top; // -1 when empty
	unsigned char pending_byte;
	int has_pending_byte;
#ifdef TERSE_ENABLE_TEST_MODE
	terse_test_state_t *test_state;
#endif
};

#endif // TERSE_HANDLE_H
