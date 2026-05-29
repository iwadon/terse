#ifndef TERSE_HANDLE_H
#define TERSE_HANDLE_H

#include "terse.h"

#ifdef TERSE_ENABLE_TEST_MODE
#include "terse_test.h"
/* Forward declaration for test state */
typedef struct terse_test_state terse_test_state_t;
#endif

#include "terse_codec.h"

/* State history stack depth macro.  Small for sanity, yet enough for
 * typical nested UI layers.
 */
#define TERSE_STATE_STACK_MAX 8

/* Internal buffer sizes for escape sequences and text processing */
#define TERSE_SMALL_BUFFER_SIZE 16  /* Very small buffers (single escape seq) */
#define TERSE_ESCAPE_BUFFER_SIZE 32 /* Small escape sequences (cursor, style) */
#define TERSE_LARGE_BUFFER_SIZE 128 /* Large buffers (DA response, style seq) */
#define TERSE_TEXT_BUFFER_SIZE 256  /* Text and header buffers */

/* Internal handle structure definition.
 * This is shared across implementation modules (terse.c, terse_output.c, etc.)
 * but NOT exposed to public API.
 */
struct terse_handle {
	terse_profile_t requested_profile;
	terse_capabilities_t capabilities;
	terse_capabilities_t detected_capabilities;
	terse_options_t options;
	terse_codec_t codec;
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
	/* Buffered rendering state (TERSE_RENDER_BUFFERED).
	 * All NULL/0 in immediate mode; allocated in terse_open() when buffered. */
	terse_render_mode_t render_mode;
	terse_cell_t *cur_cells;  /* current frame (rows*cols), row-major */
	terse_cell_t *prev_cells; /* previous frame, for diff */
	unsigned char *dirty;     /* per-cell dirty flags (rows*cols) */
	int buf_rows;             /* allocated buffer dimensions */
	int buf_cols;
	/* Virtual screen origin on the terminal (local cell coords are projected to
	 * the terminal as absolute = origin + local at flush time). Default (0,0),
	 * where local coords coincide with absolute coords (legacy behavior). */
	int buf_origin_row;
	int buf_origin_col;
	/* Rectangle emitted by the previous flush, used to erase residue when the
	 * origin moves or the buffer shrinks. prev_valid is 0 until the first flush. */
	int prev_origin_row;
	int prev_origin_col;
	int prev_buf_rows;
	int prev_buf_cols;
	int prev_valid;
	int in_flush;          /* nonzero while terse_flush() emits diff: write paths run immediately */
	int alt_screen_active; /* nonzero while in the alternate screen buffer */
	/* Platform-specific opaque state, owned by the platform layer.
	 * Allocated in terse_platform_init() and freed in terse_platform_shutdown().
	 * Platforms that need no per-handle state leave this NULL. */
	void *platform_data;
#ifdef TERSE_ENABLE_TEST_MODE
	terse_test_state_t *test_state;
#endif
};

/*
 * Common escape sequences.
 * Defined in terse.c, shared across modules.
 */
extern const char TERSE_RESET_ALL_SEQ[];
extern const char TERSE_RESET_COLOR_SEQ[];
extern const char TERSE_RESET_EFFECTS_SEQ[];

/* Lengths for use with write_sequence (excludes null terminator) */
#define TERSE_RESET_ALL_SEQ_LEN 4
#define TERSE_RESET_COLOR_SEQ_LEN 8
#define TERSE_RESET_EFFECTS_SEQ_LEN 17

/*
 * Common error handling helpers.
 * These are defined in terse.c and shared across modules.
 */
int ensure_handle(terse_handle_t handle);
void set_error(terse_handle_t handle, terse_error_t error);
void clear_error(terse_handle_t handle);

/*
 * Convenience macros for common error handling patterns.
 * Use these to reduce boilerplate in API functions.
 */

/* Check handle validity and return early if invalid */
#define TERSE_CHECK_HANDLE(h)       \
	do {                            \
		int _rc = ensure_handle(h); \
		if (_rc != 0) {             \
			return _rc;             \
		}                           \
	} while (0)

/* Check handle and clear error on success */
#define TERSE_CHECK_HANDLE_CLEAR(h) \
	do {                            \
		int _rc = ensure_handle(h); \
		if (_rc != 0) {             \
			return _rc;             \
		}                           \
		clear_error(h);             \
	} while (0)

/*
 * Convenience macros for write operations.
 * These return handle->last_error on failure.
 */

/* Write a literal string, returning on error */
#define TERSE_WRITE_LITERAL(h, lit)           \
	do {                                      \
		if (write_literal((h), (lit)) != 0) { \
			return (h)->last_error;           \
		}                                     \
	} while (0)

/* Write a sequence with length, returning on error */
#define TERSE_WRITE_SEQ(h, seq, len)                  \
	do {                                              \
		if (write_sequence((h), (seq), (len)) != 0) { \
			return (h)->last_error;                   \
		}                                             \
	} while (0)

#endif // TERSE_HANDLE_H
