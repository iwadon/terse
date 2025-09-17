#ifndef TERSE_H_INCLUDED
#define TERSE_H_INCLUDED

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

typedef struct terse_options {
	int input_fd;
	int output_fd;
	const char *codec_name;
} terse_options_t;

typedef struct terse_capabilities {
	terse_profile_t profile;
	int has_basic_output;
	int has_cursor_visibility;
	int has_move_absolute;
	int has_move_relative;
	int has_clear_line;
	int has_clear_screen;
} terse_capabilities_t;

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


#endif // TERSE_H_INCLUDED
