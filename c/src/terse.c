#include "terse.h"

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct terse_handle {
	terse_profile_t requested_profile;
	terse_capabilities_t capabilities;
	terse_options_t options;
	terse_size_t size;
	int cursor_visible;
	int cursor_row;
	int cursor_col;
	int cursor_known;
	terse_error_category_t last_error;
	int last_errno;
};

static void
set_error(terse_handle_t handle, terse_error_category_t category, int code)
{
	if (!handle) {
		return;
	}
	handle->last_error = category;
	handle->last_errno = code;
}

static void
clear_error(terse_handle_t handle)
{
	set_error(handle, TERSE_ERROR_NONE, 0);
}

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
		.has_size = 0,
		.has_sgr_basic = 0,
		.has_sgr_extended = 0,
		.has_truecolor = 0,
		.has_text_styles = 0,
		.has_mouse_tracking = 0,
		.has_bracketed_paste = 0,
		.has_title = 0,
		.has_hyperlinks = 0,
	};
	return caps;
}

static void emit_reset_sequences(terse_handle_t handle);

static terse_options_t
default_options(void)
{
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
	};
	return options;
}

int
terse_validate_options(const terse_options_t *options)
{
	if (!options) {
		return 0;
	}
	if (options->input_fd < 0 || options->output_fd < 0) {
		errno = EBADF;
		return -EBADF;
	}
	return 0;
}

static terse_size_t
make_unknown_size(void)
{
	terse_size_t size = {
		.rows = 0,
		.cols = 0,
		.known = 0,
	};
	return size;
}

static terse_size_t
query_fd_size(int fd)
{
	terse_size_t size = make_unknown_size();
	if (fd < 0) {
		return size;
	}
	struct winsize ws;
	if (ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
		size.rows = ws.ws_row;
		size.cols = ws.ws_col;
		size.known = 1;
	}
	return size;
}

static void
refresh_size(terse_handle_t handle)
{
	if (handle->options.disabled_caps & TERSE_CAP_DISABLE_SIZE) {
		handle->size = make_unknown_size();
		handle->capabilities.has_size = 0;
		return;
	}
	terse_size_t size = query_fd_size(handle->options.output_fd);
	if (!size.known && handle->options.input_fd != handle->options.output_fd) {
		size = query_fd_size(handle->options.input_fd);
	}
	if (size.known || !handle->size.known) {
		handle->size = size;
		handle->capabilities.has_size = size.known;
	}
}

terse_handle_t
terse_open(terse_profile_t requested_profile, const terse_options_t *options)
{
	if (requested_profile < TERSE_P0 || requested_profile > TERSE_P3) {
		return NULL;
	}
	if (terse_validate_options(options) < 0) {
		return NULL;
	}

	terse_handle_t handle = malloc(sizeof(*handle));
	if (!handle) {
		return NULL;
	}

	handle->requested_profile = requested_profile;
	handle->capabilities = make_p0_capabilities();

	if (options) {
		handle->options = *options;
		if (!handle->options.codec_name) {
			handle->options.codec_name = default_options().codec_name;
		}
	} else {
		handle->options = default_options();
	}
	handle->size = make_unknown_size();

	unsigned int disabled = handle->options.disabled_caps;
	if (disabled & TERSE_CAP_DISABLE_BASIC_OUTPUT) {
		handle->capabilities.has_basic_output = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_CURSOR_VISIBILITY) {
		handle->capabilities.has_cursor_visibility = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_MOVE_ABSOLUTE) {
		handle->capabilities.has_move_absolute = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_MOVE_RELATIVE) {
		handle->capabilities.has_move_relative = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_CLEAR_LINE) {
		handle->capabilities.has_clear_line = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_CLEAR_SCREEN) {
		handle->capabilities.has_clear_screen = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_SIZE) {
		handle->capabilities.has_size = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_SGR_BASIC) {
		handle->capabilities.has_sgr_basic = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_SGR_EXTENDED) {
		handle->capabilities.has_sgr_extended = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_TRUECOLOR) {
		handle->capabilities.has_truecolor = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_TEXT_STYLES) {
		handle->capabilities.has_text_styles = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_MOUSE) {
		handle->capabilities.has_mouse_tracking = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_BRACKETED_PASTE) {
		handle->capabilities.has_bracketed_paste = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_TITLE) {
		handle->capabilities.has_title = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_HYPERLINK) {
		handle->capabilities.has_hyperlinks = 0;
	}
	handle->cursor_visible = 1;
	handle->cursor_row = 0;
	handle->cursor_col = 0;
	handle->cursor_known = 0;
	clear_error(handle);
	refresh_size(handle);

	return handle;
}

void
terse_close(terse_handle_t handle)
{
	emit_reset_sequences(handle);
	free(handle);
}

terse_capabilities_t
terse_get_capabilities(terse_handle_t handle)
{
	if (!handle) {
		return make_p0_capabilities();
	}
	if (!handle->size.known) {
		refresh_size(handle);
	}
	clear_error(handle);
	return handle->capabilities;
}

static int
ensure_handle(terse_handle_t handle)
{
	if (!handle) {
		errno = EINVAL;
		return -EINVAL;
	}
	return 0;
}

static int
write_bytes(int fd, const char *bytes, size_t len)
{
	if (!bytes) {
		errno = EINVAL;
		return -EINVAL;
	}
	while (len > 0) {
		ssize_t written = write(fd, bytes, len);
		if (written < 0) {
			if (errno == EINTR) {
				continue;
			}
			int err = errno;
			errno = err;
			return -err;
		}
		if (written == 0) {
			errno = EPIPE;
			return -EPIPE;
		}
		bytes += (size_t)written;
		len -= (size_t)written;
	}
	return 0;
}

static int
write_literal(terse_handle_t handle, const char *literal)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!literal) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	int out = write_bytes(handle->options.output_fd, literal, strlen(literal));
	if (out < 0) {
		set_error(handle, TERSE_ERROR_TRANSPORT, -out);
	} else {
		clear_error(handle);
	}
	return out;
}

static int
write_sequence(terse_handle_t handle, const char *sequence, size_t length)
{
	int out = write_bytes(handle->options.output_fd, sequence, length);
	if (out < 0) {
		set_error(handle, TERSE_ERROR_TRANSPORT, -out);
	} else {
		clear_error(handle);
	}
	return out;
}

static void
emit_reset_sequences(terse_handle_t handle)
{
	if (!handle) {
		return;
	}
	if (!handle->capabilities.has_basic_output) {
		return;
	}
	static const char *const cursor_on_seq = "\x1b[?25h";
	static const char *const reset_seq = "\x1b[0m";
	if (!handle->cursor_visible) {
		if (write_sequence(handle, cursor_on_seq, strlen(cursor_on_seq)) == 0) {
			handle->cursor_visible = 1;
		}
	}
	write_sequence(handle, reset_seq, strlen(reset_seq));
}

static int
wait_for_input(int fd, int timeout_ms)
{
	struct pollfd pfd = {
		.fd = fd,
		.events = POLLIN,
	};
	int poll_timeout = timeout_ms < 0 ? -1 : timeout_ms;
	for (;;) {
		int ready = poll(&pfd, 1, poll_timeout);
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			int err = errno;
			errno = err;
			return -err;
		}
		return ready;
	}
}

static ssize_t
read_byte(int fd, unsigned char *out)
{
	for (;;) {
		ssize_t n = read(fd, out, 1);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			int err = errno;
			errno = err;
			return -err;
		}
		return n;
	}
}

static size_t
drain_escape_sequence(int fd, unsigned char *buffer, size_t max)
{
	size_t len = 1;
	while (len < max) {
		struct pollfd pfd = {
			.fd = fd,
			.events = POLLIN,
		};
		int ready = poll(&pfd, 1, 10);
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		if (ready == 0) {
			break;
		}
		ssize_t n = read(fd, buffer + len, 1);
		if (n <= 0) {
			break;
		}
		len += (size_t)n;
		if (len >= 3) {
			unsigned char c = buffer[len - 1];
			if (c >= '@' && c <= '~') {
				break;
			}
		}
	}
	return len;
}

static int
parse_csi_sequence(const unsigned char *seq, size_t len, int *values, size_t max_values, size_t *value_count, char *final)
{
	if (len < 2 || seq[0] != 0x1b || seq[1] != '[') {
		return -1;
	}
	if (len < 3) {
		return -1;
	}
	char terminator = (char)seq[len - 1];
	if (terminator < '@' || terminator > '~') {
		return -1;
	}
	size_t count = 0;
	size_t index = 2;
	while (index < len - 1) {
		int value = 0;
		int has_digit = 0;
		while (index < len - 1 && isdigit((unsigned char)seq[index])) {
			has_digit = 1;
			value = (value * 10) + (seq[index] - '0');
			++index;
		}
		if (has_digit) {
			if (count < max_values) {
				values[count] = value;
			}
			++count;
		}
		if (index >= len - 1) {
			break;
		}
		if (seq[index] == ';') {
			++index;
			continue;
		}
		break;
	}
	*value_count = count;
	*final = terminator;
	return 0;
}

static int
modifier_bits_from_param(int param)
{
	int mods = 0;
	switch (param) {
	case 2:
		mods = TERSE_MOD_SHIFT;
		break;
	case 3:
		mods = TERSE_MOD_ALT;
		break;
	case 4:
		mods = TERSE_MOD_SHIFT | TERSE_MOD_ALT;
		break;
	case 5:
		mods = TERSE_MOD_CTRL;
		break;
	case 6:
		mods = TERSE_MOD_SHIFT | TERSE_MOD_CTRL;
		break;
	case 7:
		mods = TERSE_MOD_ALT | TERSE_MOD_CTRL;
		break;
	case 8:
		mods = TERSE_MOD_SHIFT | TERSE_MOD_ALT | TERSE_MOD_CTRL;
		break;
	default:
		break;
	}
	return mods;
}

int
terse_clear_screen(terse_handle_t handle, terse_clear_mode_t mode)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_clear_screen) {
		clear_error(handle);
		return 0;
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
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}

	return write_literal(handle, sequence);
}

int
terse_clear_line(terse_handle_t handle, terse_clear_mode_t mode)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_clear_line) {
		clear_error(handle);
		return 0;
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
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}

	return write_literal(handle, sequence);
}

int
terse_move_to(terse_handle_t handle, int row, int col)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_move_absolute) {
		clear_error(handle);
		return 0;
	}

	if (row < 1) {
		row = 1;
	}
	if (col < 1) {
		col = 1;
	}
	if (row == handle->cursor_row && col == handle->cursor_col) {
		clear_error(handle);
		return 0;
	}

	char sequence[32];
	int written = snprintf(sequence, sizeof(sequence), "\x1b[%d;%dH", row, col);
	if (written <= 0 || (size_t)written >= sizeof(sequence)) {
		errno = EINVAL;
		return -EINVAL;
	}

	int out = write_sequence(handle, sequence, (size_t)written);
	if (out == 0) {
		handle->cursor_row = row;
		handle->cursor_col = col;
		handle->cursor_known = 1;
	}
	return out;
}

int
terse_move_by(terse_handle_t handle, int drow, int dcol)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_move_relative) {
		clear_error(handle);
		return 0;
	}
	if (drow == 0 && dcol == 0) {
		clear_error(handle);
		return 0;
	}

	int new_row = handle->cursor_row;
	int new_col = handle->cursor_col;

	if (drow < 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dA", -drow);
		if (len <= 0) {
			errno = EINVAL;
			return -EINVAL;
		}
		int w = write_sequence(handle, seq, (size_t)len);
		if (w < 0) {
			return w;
		}
		new_row += drow;
	} else if (drow > 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dB", drow);
		if (len <= 0) {
			errno = EINVAL;
			return -EINVAL;
		}
		int w = write_sequence(handle, seq, (size_t)len);
		if (w < 0) {
			return w;
		}
		new_row += drow;
	}

	if (dcol < 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dD", -dcol);
		if (len <= 0) {
			errno = EINVAL;
			return -EINVAL;
		}
		int w = write_sequence(handle, seq, (size_t)len);
		if (w < 0) {
			return w;
		}
		new_col += dcol;
	} else if (dcol > 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dC", dcol);
		if (len <= 0) {
			errno = EINVAL;
			return -EINVAL;
		}
		int w = write_sequence(handle, seq, (size_t)len);
		if (w < 0) {
			return w;
		}
		new_col += dcol;
	}

	if (new_row < 1) {
		new_row = 1;
	}
	if (new_col < 1) {
		new_col = 1;
	}
	handle->cursor_row = new_row;
	handle->cursor_col = new_col;
	handle->cursor_known = 1;
	clear_error(handle);
	return 0;
}

int
terse_show_cursor(terse_handle_t handle, int visible)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_cursor_visibility) {
		clear_error(handle);
		return 0;
	}
	int target = visible ? 1 : 0;
	if (handle->cursor_visible == target) {
		clear_error(handle);
		return 0;
	}
	int result = write_literal(handle, target ? "\x1b[?25h" : "\x1b[?25l");
	if (result == 0) {
		handle->cursor_visible = target;
	}
	return result;
}

int
terse_write_text(terse_handle_t handle, const char *graphemes)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!graphemes) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	if (!handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}

	return write_literal(handle, graphemes);
}

int
terse_flush(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	clear_error(handle);
	return 0;
}

static void
set_key_event(terse_event_t *event, terse_event_type_t type, int mods)
{
	event->type = type;
	event->data.key.mods = mods;
}

static void
set_char_event(terse_event_t *event, unsigned int scalar, int mods)
{
	event->type = TERSE_EVENT_CHAR;
	event->data.ch.scalar = scalar;
	event->data.ch.width = 1;
	event->data.ch.mods = mods;
}

static void
set_raw_event(terse_event_t *event, const unsigned char *bytes, size_t length)
{
	event->type = TERSE_EVENT_RAW_SEQUENCE;
	if (length > TERSE_EVENT_RAW_MAX) {
		length = TERSE_EVENT_RAW_MAX;
	}
	event->data.raw.length = length;
	memset(event->data.raw.bytes, 0, TERSE_EVENT_RAW_MAX);
	memcpy(event->data.raw.bytes, bytes, length);
}

static void
set_resize_event(terse_event_t *event, int rows, int cols)
{
	event->type = TERSE_EVENT_RESIZE;
	event->data.resize.rows = rows;
	event->data.resize.cols = cols;
}

int
terse_read_event(terse_handle_t handle, int timeout_ms, terse_event_t *out_event)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!out_event) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}

	int fd = handle->options.input_fd;
	int ready = wait_for_input(fd, timeout_ms);
	if (ready == 0) {
		clear_error(handle);
		return TERSE_EVENT_NONE;
	}
	if (ready < 0) {
		int err = -ready;
		set_error(handle, TERSE_ERROR_TRANSPORT, err);
		return -err;
	}

	unsigned char first = 0;
	ssize_t n = read_byte(fd, &first);
	if (n == 0) {
		errno = EPIPE;
		set_error(handle, TERSE_ERROR_TRANSPORT, EPIPE);
		return -EPIPE;
	}
	if (n < 0) {
		int err = -(int)n;
		set_error(handle, TERSE_ERROR_TRANSPORT, err);
		return (int)n;
	}

	switch (first) {
	case '\r':
	case '\n':
		set_key_event(out_event, TERSE_EVENT_ENTER, 0);
		clear_error(handle);
		return TERSE_EVENT_OK;
	case '\b':
	case 0x7f:
		set_key_event(out_event, TERSE_EVENT_BACKSPACE, 0);
		clear_error(handle);
		return TERSE_EVENT_OK;
	case '\t':
		set_key_event(out_event, TERSE_EVENT_TAB, 0);
		clear_error(handle);
		return TERSE_EVENT_OK;
	default:
		break;
	}

	if (first >= 0x01 && first <= 0x1a) {
		unsigned int scalar = 'A' + (first - 1);
		set_char_event(out_event, scalar, TERSE_MOD_CTRL);
		return TERSE_EVENT_OK;
	}

	if (first == 0x1b) {
		unsigned char seq[TERSE_EVENT_RAW_MAX] = { 0 };
		seq[0] = first;
		size_t len = drain_escape_sequence(fd, seq, TERSE_EVENT_RAW_MAX);
		int values[8] = { 0 };
		size_t value_count = 0;
		char final = 0;
		if (parse_csi_sequence(seq, len, values, 8, &value_count, &final) == 0) {
			if (final == 't' && value_count >= 3 && values[0] == 8) {
				set_resize_event(out_event, values[1], values[2]);
				handle->size.rows = values[1];
				handle->size.cols = values[2];
				handle->size.known = 1;
				handle->capabilities.has_size = 1;
				clear_error(handle);
				return TERSE_EVENT_OK;
			}
			if (final == 'A' || final == 'B' || final == 'C' || final == 'D') {
				int mods = 0;
				if (value_count > 0) {
					mods = modifier_bits_from_param(values[value_count - 1]);
				}
				switch (final) {
				case 'A':
					set_key_event(out_event, TERSE_EVENT_ARROW_UP, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 'B':
					set_key_event(out_event, TERSE_EVENT_ARROW_DOWN, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 'C':
					set_key_event(out_event, TERSE_EVENT_ARROW_RIGHT, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 'D':
					set_key_event(out_event, TERSE_EVENT_ARROW_LEFT, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				default:
					break;
				}
			}
		}
		set_raw_event(out_event, seq, len);
		clear_error(handle);
		return TERSE_EVENT_OK;
	}

	if (first >= 0x20 && first <= 0x7e) {
		set_char_event(out_event, first, 0);
		clear_error(handle);
		return TERSE_EVENT_OK;
	}

	unsigned char raw_bytes[1] = { first };
	set_raw_event(out_event, raw_bytes, 1);
	clear_error(handle);
	return TERSE_EVENT_OK;
}

terse_size_t
terse_get_size(terse_handle_t handle)
{
	terse_size_t unknown = make_unknown_size();
	if (ensure_handle(handle) < 0) {
		return unknown;
	}
	if (handle->options.disabled_caps & TERSE_CAP_DISABLE_SIZE) {
		clear_error(handle);
		return unknown;
	}
	if (!handle->size.known) {
		refresh_size(handle);
	}
	clear_error(handle);
	return handle->size;
}

int
terse_get_options(terse_handle_t handle, terse_options_t *out_options)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!out_options) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	*out_options = handle->options;
	clear_error(handle);
	return 0;
}

terse_error_info_t
terse_get_last_error(terse_handle_t handle)
{
	terse_error_info_t info = { TERSE_ERROR_NONE, 0 };
	if (!handle) {
		info.category = TERSE_ERROR_STATE;
		info.code = EINVAL;
		return info;
	}
	info.category = handle->last_error;
	info.code = handle->last_errno;
	return info;
}

int
terse_capture_state(terse_handle_t handle, terse_state_t *out_state)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!out_state) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	out_state->cursor_known = handle->cursor_known;
	out_state->cursor_visible = handle->cursor_visible;
	out_state->cursor_row = handle->cursor_row;
	out_state->cursor_col = handle->cursor_col;
	clear_error(handle);
	return 0;
}

int
terse_restore_state(terse_handle_t handle, const terse_state_t *state)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!state) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	int result = 0;
	if (state->cursor_known) {
		if (handle->capabilities.has_move_absolute && state->cursor_row > 0 && state->cursor_col > 0) {
			int move_rc = terse_move_to(handle, state->cursor_row, state->cursor_col);
			if (move_rc < 0 && result == 0) {
				result = move_rc;
			}
		}
	}
	if (handle->capabilities.has_cursor_visibility) {
		int want_visible = state->cursor_known ? state->cursor_visible : 1;
		int vis_rc = terse_show_cursor(handle, want_visible);
		if (vis_rc < 0 && result == 0) {
			result = vis_rc;
		}
	}
	return result;
}
