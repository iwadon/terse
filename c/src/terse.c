#include "terse.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
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

static int
write_bytes(int fd, const char *bytes, size_t len)
{
	if (!bytes) {
		return -1;
	}
	while (len > 0) {
		ssize_t written = write(fd, bytes, len);
		if (written < 0) {
			if (errno == EINTR) {
				continue;
			}
			return -1;
		}
		if (written == 0) {
			return -1;
		}
		bytes += (size_t)written;
		len -= (size_t)written;
	}
	return 0;
}

static int
write_literal(terse_handle_t handle, const char *literal)
{
	if (!handle || !literal) {
		return -1;
	}
	return write_bytes(handle->options.output_fd, literal, strlen(literal));
}

int
terse_clear_screen(terse_handle_t handle, terse_clear_mode_t mode)
{
	if (ensure_handle(handle) != 0) {
		return -1;
	}

	const char *sequence = NULL;
	switch (mode) {
	case TERSE_CLEAR_AFTER:
		sequence = "\x1b[J";
		break;
	case TERSE_CLEAR_BEFORE:
		sequence = "\x1b[1J";
		break;
	case TERSE_CLEAR_ALL:
		sequence = "\x1b[2J";
		break;
	default:
		return -1;
	}

	return write_literal(handle, sequence);
}

int
terse_clear_line(terse_handle_t handle, terse_clear_mode_t mode)
{
	if (ensure_handle(handle) != 0) {
		return -1;
	}

	const char *sequence = NULL;
	switch (mode) {
	case TERSE_CLEAR_AFTER:
		sequence = "\x1b[K";
		break;
	case TERSE_CLEAR_BEFORE:
		sequence = "\x1b[1K";
		break;
	case TERSE_CLEAR_ALL:
		sequence = "\x1b[2K";
		break;
	default:
		return -1;
	}

	return write_literal(handle, sequence);
}

int
terse_move_to(terse_handle_t handle, int row, int col)
{
	if (ensure_handle(handle) != 0) {
		return -1;
	}

	if (row < 1) {
		row = 1;
	}
	if (col < 1) {
		col = 1;
	}

	char sequence[32];
	int written = snprintf(sequence, sizeof(sequence), "\x1b[%d;%dH", row, col);
	if (written <= 0 || (size_t)written >= sizeof(sequence)) {
		return -1;
	}

	return write_bytes(handle->options.output_fd, sequence, (size_t)written);
}

int
terse_move_by(terse_handle_t handle, int drow, int dcol)
{
	if (ensure_handle(handle) != 0) {
		return -1;
	}

	int fd = handle->options.output_fd;

	if (drow < 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dA", -drow);
		if (len <= 0 || write_bytes(fd, seq, (size_t)len) != 0) {
			return -1;
		}
	} else if (drow > 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dB", drow);
		if (len <= 0 || write_bytes(fd, seq, (size_t)len) != 0) {
			return -1;
		}
	}

	if (dcol < 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dD", -dcol);
		if (len <= 0 || write_bytes(fd, seq, (size_t)len) != 0) {
			return -1;
		}
	} else if (dcol > 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dC", dcol);
		if (len <= 0 || write_bytes(fd, seq, (size_t)len) != 0) {
			return -1;
		}
	}

	return 0;
}

int
terse_show_cursor(terse_handle_t handle, int visible)
{
	if (ensure_handle(handle) != 0) {
		return -1;
	}

	return write_literal(handle, visible ? "\x1b[?25h" : "\x1b[?25l");
}

int
terse_write_text(terse_handle_t handle, const char *graphemes)
{
	if (!handle || !graphemes) {
		return -1;
	}

	return write_literal(handle, graphemes);
}

int
terse_flush(terse_handle_t handle)
{
	return ensure_handle(handle);
}
