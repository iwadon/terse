#include "terse.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct terse_handle {
	terse_profile_t requested_profile;
	terse_capabilities_t capabilities;
	terse_options_t options;
};

static terse_capabilities_t
make_p0_capabilities(void)
{
	terse_capabilities_t caps = {
		.profile = TERSE_P0,
		.has_basic_output = 1,
		.has_cursor_visibility = 1,
		.has_move_absolute = 1,
		.has_move_relative = 1,
		.has_clear_line = 1,
		.has_clear_screen = 1,
	};
	return caps;
}

static terse_options_t
default_options(void)
{
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
	};
	return options;
}

static terse_capabilities_t
derive_capabilities(terse_profile_t requested_profile)
{
	(void)requested_profile;
	return make_p0_capabilities();
}

terse_handle_t
terse_open(terse_profile_t requested_profile, const terse_options_t *options)
{
	if (requested_profile < TERSE_P0 || requested_profile > TERSE_P3) {
		return NULL;
	}

	terse_handle_t handle = malloc(sizeof(*handle));
	if (!handle) {
		return NULL;
	}

	handle->requested_profile = requested_profile;
	handle->capabilities = derive_capabilities(requested_profile);

	if (options) {
		handle->options = *options;
		if (!handle->options.codec_name) {
			handle->options.codec_name = default_options().codec_name;
		}
	} else {
		handle->options = default_options();
	}

	return handle;
}

void
terse_close(terse_handle_t handle)
{
	free(handle);
}

terse_capabilities_t
terse_get_capabilities(terse_handle_t handle)
{
	if (!handle) {
		return make_p0_capabilities();
	}
	return handle->capabilities;
}

static int
ensure_handle(terse_handle_t handle)
{
	return handle ? 0 : -1;
}

int
terse_clear_screen(terse_handle_t handle, terse_clear_mode_t mode)
{
	(void)mode;
	return ensure_handle(handle);
}

int
terse_clear_line(terse_handle_t handle, terse_clear_mode_t mode)
{
	(void)mode;
	return ensure_handle(handle);
}

int
terse_move_to(terse_handle_t handle, int row, int col)
{
	(void)row;
	(void)col;
	return ensure_handle(handle);
}

int
terse_move_by(terse_handle_t handle, int drow, int dcol)
{
	(void)drow;
	(void)dcol;
	return ensure_handle(handle);
}

int
terse_show_cursor(terse_handle_t handle, int visible)
{
	(void)visible;
	return ensure_handle(handle);
}

int
terse_write_text(terse_handle_t handle, const char *graphemes)
{
	if (!handle || !graphemes) {
		return -1;
	}

	size_t remaining = strlen(graphemes);
	const char *cursor = graphemes;
	int fd = handle->options.output_fd;

	while (remaining > 0) {
		ssize_t written = write(fd, cursor, remaining);
		if (written < 0) {
			if (errno == EINTR) {
				continue;
			}
			return -1;
		}
		if (written == 0) {
			return -1;
		}
		cursor += (size_t)written;
		remaining -= (size_t)written;
	}

	return 0;
}

int
terse_flush(terse_handle_t handle)
{
	return ensure_handle(handle);
}
