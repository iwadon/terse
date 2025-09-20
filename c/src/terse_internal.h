#ifndef TERSE_INTERNAL_H_INCLUDED
#define TERSE_INTERNAL_H_INCLUDED

#include "terse.h"
#include <stddef.h>
#include <sys/types.h>

/* Internal constants */
#define TERSE_STATE_STACK_MAX 8

/* Internal RGB type */
typedef struct terse_rgb {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} terse_rgb_t;

/* ANSI escape sequences */
extern const char TERSE_RESET_ALL_SEQ[];
extern const char TERSE_RESET_COLOR_SEQ[];
extern const char TERSE_RESET_EFFECTS_SEQ[];
extern const char BASE64_ALPHABET[];

/* Basic color RGB values */
extern const terse_rgb_t basic16_rgb[16];

/* Forward declarations */
typedef struct terse_handle *terse_handle_t;

/* Internal handle structure */
struct terse_handle {
	terse_profile_t requested_profile;
	terse_capabilities_t capabilities;
	terse_capabilities_t detected_capabilities;
	terse_options_t options;
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
	terse_error_category_t last_error;
	int last_errno;
	unsigned int runtime_enabled;
	unsigned int runtime_disabled;
	// State history
	terse_state_t state_stack[TERSE_STATE_STACK_MAX];
	int state_stack_top; // -1 when empty
};

/* Error handling */
void set_error(terse_handle_t handle, terse_error_category_t category, int code);
void clear_error(terse_handle_t handle);

/* Handle validation */
int ensure_handle(terse_handle_t handle);

/* I/O helpers */
int write_bytes(int fd, const char *bytes, size_t len);
int write_literal(terse_handle_t handle, const char *literal);
int write_sequence(terse_handle_t handle, const char *sequence, size_t length);

/* Color and Style API (in terse_color.c) */
terse_color_t terse_color_default(void);
terse_color_t terse_color_basic(terse_basic_color_t color, int bright);
terse_color_t terse_color_palette(unsigned char index);
terse_color_t terse_color_truecolor(unsigned char r, unsigned char g, unsigned char b);
terse_style_t terse_style_default(void);

/* Color/Style helper functions (in terse_color.c) */
int styles_equal(const terse_style_t *a, const terse_style_t *b);
unsigned int mask_effects(unsigned int effects);

/* Style helpers */
void update_effective_style(terse_handle_t handle);
terse_style_t sanitize_style_request(const terse_style_t *style);
terse_style_t make_effective_style(const terse_capabilities_t *caps, const terse_style_t *requested);

/* Capability helpers */
void recompute_capabilities(terse_handle_t handle);
void apply_runtime_overrides(terse_handle_t handle);

/* Size management */
void refresh_size(terse_handle_t handle);

#endif /* TERSE_INTERNAL_H_INCLUDED */