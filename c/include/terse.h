#ifndef TERSE_H_INCLUDED
#define TERSE_H_INCLUDED

#include <stddef.h>

typedef struct terse_handle *terse_handle_t;

typedef enum terse_profile {
	TERSE_P0 = 0,
	TERSE_P1,
	TERSE_P2,
	TERSE_P3
} terse_profile_t;

typedef enum terse_clear_mode {
	TERSE_CLEAR_AFTER = 0,
	TERSE_CLEAR_BEFORE,
	TERSE_CLEAR_ALL
} terse_clear_mode_t;

typedef enum terse_color_support {
	TERSE_COLOR_NONE = 0,
	TERSE_COLOR_BASIC16,
	TERSE_COLOR_PALETTE256,
	TERSE_COLOR_TRUECOLOR
} terse_color_support_t;

typedef enum terse_color_kind {
	TERSE_COLOR_KIND_DEFAULT = 0,
	TERSE_COLOR_KIND_BASIC16,
	TERSE_COLOR_KIND_PALETTE256,
	TERSE_COLOR_KIND_TRUECOLOR
} terse_color_kind_t;

typedef enum terse_basic_color {
	TERSE_BASIC_COLOR_BLACK = 0,
	TERSE_BASIC_COLOR_RED,
	TERSE_BASIC_COLOR_GREEN,
	TERSE_BASIC_COLOR_YELLOW,
	TERSE_BASIC_COLOR_BLUE,
	TERSE_BASIC_COLOR_MAGENTA,
	TERSE_BASIC_COLOR_CYAN,
	TERSE_BASIC_COLOR_WHITE
} terse_basic_color_t;

typedef struct terse_color {
	terse_color_kind_t kind;
	union {
		struct {
			unsigned char value;
		} palette;
		struct {
			unsigned char r;
			unsigned char g;
			unsigned char b;
		} truecolor;
		struct {
			terse_basic_color_t color;
			int bright;
		} basic16;
	} data;
} terse_color_t;

typedef struct terse_style {
	terse_color_t foreground;
	terse_color_t background;
	unsigned int effects;
} terse_style_t;

typedef enum terse_reset_scope {
	TERSE_RESET_ALL = 0,
	TERSE_RESET_COLOR_ONLY,
	TERSE_RESET_EFFECTS_ONLY
} terse_reset_scope_t;

typedef struct terse_capabilities {
	terse_profile_t profile;
	int has_basic_output;
	int has_cursor_visibility;
	int has_move_absolute;
	int has_move_relative;
	int has_clear_line;
	int has_clear_screen;
	int has_size;
	int has_sgr_basic;
	int has_sgr_extended;
	int has_truecolor;
	int has_text_styles;
	int has_mouse_tracking;
	int has_bracketed_paste;
	int has_title;
	int has_hyperlinks;
	terse_color_support_t colors;
	unsigned int effects;
} terse_capabilities_t;

typedef enum terse_capability_flag {
	TERSE_CAP_DISABLE_BASIC_OUTPUT = 1u << 0,
	TERSE_CAP_DISABLE_CURSOR_VISIBILITY = 1u << 1,
	TERSE_CAP_DISABLE_MOVE_ABSOLUTE = 1u << 2,
	TERSE_CAP_DISABLE_MOVE_RELATIVE = 1u << 3,
	TERSE_CAP_DISABLE_CLEAR_LINE = 1u << 4,
	TERSE_CAP_DISABLE_CLEAR_SCREEN = 1u << 5,
	TERSE_CAP_DISABLE_SIZE = 1u << 6,
	TERSE_CAP_DISABLE_SGR_BASIC = 1u << 7,
	TERSE_CAP_DISABLE_SGR_EXTENDED = 1u << 8,
	TERSE_CAP_DISABLE_TRUECOLOR = 1u << 9,
	TERSE_CAP_DISABLE_TEXT_STYLES = 1u << 10,
	TERSE_CAP_DISABLE_MOUSE = 1u << 11,
	TERSE_CAP_DISABLE_BRACKETED_PASTE = 1u << 12,
	TERSE_CAP_DISABLE_TITLE = 1u << 13,
	TERSE_CAP_DISABLE_HYPERLINK = 1u << 14,
} terse_capability_flag_t;

typedef enum terse_capability_enable_flag {
	TERSE_CAP_ENABLE_SGR_BASIC = 1u << 0,
	TERSE_CAP_ENABLE_TEXT_STYLES = 1u << 1,
	TERSE_CAP_ENABLE_SGR_EXTENDED = 1u << 2,
	TERSE_CAP_ENABLE_TRUECOLOR = 1u << 3,
	TERSE_CAP_ENABLE_MOUSE = 1u << 4,
	TERSE_CAP_ENABLE_BRACKETED_PASTE = 1u << 5,
	TERSE_CAP_ENABLE_TITLE = 1u << 6,
	TERSE_CAP_ENABLE_HYPERLINK = 1u << 7
} terse_capability_enable_flag_t;

typedef struct terse_options {
	int input_fd;
	int output_fd;
	const char *codec_name;
	unsigned int disabled_caps;
	unsigned int enabled_caps;
} terse_options_t;

typedef struct terse_size {
	int rows;
	int cols;
	int known;
} terse_size_t;

typedef struct terse_state {
	int cursor_known;
	int cursor_visible;
	int cursor_row;
	int cursor_col;
	int style_known;
	terse_style_t style;
} terse_state_t;

enum {
	TERSE_STYLE_BOLD = 1u << 0,
	TERSE_STYLE_FAINT = 1u << 1,
	TERSE_STYLE_ITALIC = 1u << 2,
	TERSE_STYLE_UNDERLINE = 1u << 3,
	TERSE_STYLE_INVERSE = 1u << 4,
	TERSE_STYLE_BLINK = 1u << 5,
	TERSE_STYLE_STRIKE = 1u << 6
};

typedef enum terse_error_category {
	TERSE_ERROR_NONE = 0,
	TERSE_ERROR_TRANSPORT,
	TERSE_ERROR_PROTOCOL,
	TERSE_ERROR_RESOURCE,
	TERSE_ERROR_CONFIG,
	TERSE_ERROR_STATE
} terse_error_category_t;

typedef struct terse_error_info {
	terse_error_category_t category;
	int code;
} terse_error_info_t;

enum {
	TERSE_MOD_SHIFT = (1 << 0),
	TERSE_MOD_CTRL = (1 << 1),
	TERSE_MOD_ALT = (1 << 2),
	TERSE_MOD_META = (1 << 3),
};

typedef enum terse_event_type {
	TERSE_EVENT_CHAR = 0,
	TERSE_EVENT_ENTER,
	TERSE_EVENT_BACKSPACE,
	TERSE_EVENT_TAB,
	TERSE_EVENT_ARROW_UP,
	TERSE_EVENT_ARROW_DOWN,
	TERSE_EVENT_ARROW_LEFT,
	TERSE_EVENT_ARROW_RIGHT,
	TERSE_EVENT_RESIZE,
	TERSE_EVENT_RAW_SEQUENCE
} terse_event_type_t;

#define TERSE_EVENT_RAW_MAX 32

typedef struct terse_event {
	terse_event_type_t type;
	union {
		struct {
			unsigned int scalar;
			int width;
			int mods;
		} ch;
		struct {
			int mods;
		} key;
		struct {
			int rows;
			int cols;
		} resize;
		struct {
			size_t length;
			unsigned char bytes[TERSE_EVENT_RAW_MAX];
		} raw;
	} data;
} terse_event_t;

enum {
	TERSE_EVENT_OK = 0,
	TERSE_EVENT_NONE = 1
};

terse_handle_t terse_open(terse_profile_t requested_profile, const terse_options_t *options);
void terse_close(terse_handle_t handle);

terse_capabilities_t terse_get_capabilities(terse_handle_t handle);

int terse_clear_screen(terse_handle_t handle, terse_clear_mode_t mode);
int terse_clear_line(terse_handle_t handle, terse_clear_mode_t mode);
int terse_move_to(terse_handle_t handle, int row, int col);
int terse_move_by(terse_handle_t handle, int drow, int dcol);
int terse_show_cursor(terse_handle_t handle, int visible);
int terse_write_text(terse_handle_t handle, const char *graphemes);
int terse_flush(terse_handle_t handle);
int terse_read_event(terse_handle_t handle, int timeout_ms, terse_event_t *out_event);
terse_size_t terse_get_size(terse_handle_t handle);
int terse_get_options(terse_handle_t handle, terse_options_t *out_options);
int terse_validate_options(const terse_options_t *options);
terse_error_info_t terse_get_last_error(terse_handle_t handle);
int terse_capture_state(terse_handle_t handle, terse_state_t *out_state);
int terse_restore_state(terse_handle_t handle, const terse_state_t *state);
terse_style_t terse_style_default(void);
terse_color_t terse_color_default(void);
terse_color_t terse_color_basic(terse_basic_color_t color, int bright);
terse_color_t terse_color_palette(unsigned char index);
terse_color_t terse_color_truecolor(unsigned char r, unsigned char g, unsigned char b);
int terse_set_style(terse_handle_t handle, const terse_style_t *style);
int terse_reset_style(terse_handle_t handle, terse_reset_scope_t scope);

#endif // TERSE_H_INCLUDED
