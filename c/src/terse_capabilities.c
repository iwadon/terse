#include "terse_internal.h"
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static size_t
probe_secondary_da(int input_fd, int output_fd, unsigned char *buffer, size_t capacity)
{
	if (!buffer || capacity == 0) {
		return 0;
	}
	if (input_fd < 0 || output_fd < 0) {
		return 0;
	}
	const char *da_query = "\x1b[>c";
	ssize_t written = write(output_fd, da_query, strlen(da_query));
	if (written < 0) {
		return 0;
	}
	struct pollfd pfd = {
		.fd = input_fd,
		.events = POLLIN,
	};
	int ready = poll(&pfd, 1, 200);
	if (ready <= 0) {
		return 0;
	}
	ssize_t read_bytes = read(input_fd, buffer, capacity - 1);
	if (read_bytes <= 0) {
		return 0;
	}
	buffer[read_bytes] = '\0';
	return (size_t)read_bytes;
}

static int
matches_da_prefix(const unsigned char *buffer, size_t length, const char *prefix)
{
	if (!buffer || !prefix || length == 0) {
		return 0;
	}
	size_t prefix_len = strlen(prefix);
	if (length < prefix_len) {
		return 0;
	}
	return strncmp((const char *)buffer, prefix, prefix_len) == 0;
}

static terse_capabilities_t
make_terminal_app_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_p0_capabilities();
	caps.profile = TERSE_P1;
	caps.has_size = 1;
	caps.has_sgr_basic = 1;
	caps.has_sgr_extended = 1;
	caps.has_text_styles = 1;
	caps.colors = has_truecolor ? TERSE_COLOR_TRUECOLOR : TERSE_COLOR_PALETTE256;
	caps.effects = TERSE_STYLE_ALL_SUPPORTED;
	caps.mouse = TERSE_MOUSE_SGR;
	caps.has_bracketed_paste = 1;
	caps.has_title = 1;
	caps.has_hyperlinks = 1;
	return caps;
}

static terse_capabilities_t
make_vte_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_terminal_app_capabilities(has_truecolor);
	caps.has_cursor_shape = 1;
	caps.images = TERSE_IMAGE_ITERM_INLINE;
	caps.notifications = 1; // Basic notifications
	return caps;
}

static terse_capabilities_t
make_iterm_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_vte_capabilities(has_truecolor);
	caps.profile = TERSE_P2;
	caps.has_clipboard_write = 1;
	caps.images = TERSE_IMAGE_ITERM_INLINE;
	caps.notifications = 1; // Basic notifications
	return caps;
}

static terse_capabilities_t
make_wezterm_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_vte_capabilities(has_truecolor);
	caps.profile = TERSE_P2;
	caps.has_clipboard_write = 1;
	caps.images = TERSE_IMAGE_ITERM_INLINE;
	caps.notifications = 1; // Basic notifications
	return caps;
}

static terse_capabilities_t
make_kitty_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_vte_capabilities(has_truecolor);
	caps.profile = TERSE_P3;
	caps.images = TERSE_IMAGE_ITERM_INLINE; // Use available constant
	return caps;
}

static terse_capabilities_t
make_ghostty_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_vte_capabilities(has_truecolor);
	caps.profile = TERSE_P2;
	caps.images = TERSE_IMAGE_ITERM_INLINE; // Use available constant
	return caps;
}

static terse_capabilities_t
make_warp_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_terminal_app_capabilities(has_truecolor);
	caps.profile = TERSE_P2;
	return caps;
}

static void
clamp_capabilities_to_request(terse_capabilities_t *caps, terse_profile_t requested)
{
	if (!caps) {
		return;
	}
	switch (requested) {
	case TERSE_P0:
		*caps = make_p0_capabilities();
		break;
	case TERSE_P1:
		if (caps->profile > TERSE_P1) {
			caps->profile = TERSE_P1;
			caps->has_clipboard_write = 0;
			caps->images = TERSE_IMAGE_NONE;
			caps->notifications = 0;
		}
		break;
	case TERSE_P2:
		if (caps->profile > TERSE_P2) {
			caps->profile = TERSE_P2;
			if (caps->images == TERSE_IMAGE_ITERM_INLINE) {
				caps->images = TERSE_IMAGE_ITERM_INLINE; // Keep same for now
			}
		}
		break;
	case TERSE_P3:
	default:
		break;
	}
}

terse_capabilities_t
detect_environment_capabilities(terse_profile_t requested_profile, const terse_options_t *options)
{
	terse_capabilities_t caps = make_p0_capabilities();
	int has_truecolor = 0;
	const char *term = getenv("TERM");
	const char *term_program = getenv("TERM_PROGRAM");
	const char *term_program_version = getenv("TERM_PROGRAM_VERSION");
	const char *colorterm = getenv("COLORTERM");
	if (colorterm && (strstr(colorterm, "truecolor") || strstr(colorterm, "24bit"))) {
		has_truecolor = 1;
	}
	if (term && strstr(term, "256color")) {
		has_truecolor = 0;
	}
	if (term && strstr(term, "truecolor")) {
		has_truecolor = 1;
	}
	unsigned char da_buffer[256];
	size_t da_length = 0;
	if (options && options->input_fd >= 0 && options->output_fd >= 0) {
		da_length = probe_secondary_da(options->input_fd, options->output_fd, da_buffer, sizeof(da_buffer));
	}
	int is_terminal_app = 0;
	int is_warp = 0;
	int is_iterm = 0;
	int is_vte = 0;
	int is_wezterm = 0;
	int is_kitty = 0;
	int is_ghostty = 0;
	if (term_program) {
		if (strcmp(term_program, "Apple_Terminal") == 0) {
			is_terminal_app = 1;
		} else if (strcmp(term_program, "Warp") == 0) {
			is_warp = 1;
		} else if (strcmp(term_program, "iTerm.app") == 0) {
			is_iterm = 1;
		} else if (strcmp(term_program, "vte") == 0) {
			is_vte = 1;
		} else if (strcmp(term_program, "WezTerm") == 0) {
			is_wezterm = 1;
		}
	}
	if (da_length > 0) {
		if (matches_da_prefix(da_buffer, da_length, "\x1b[>1;")) {
			is_vte = 1;
		} else if (matches_da_prefix(da_buffer, da_length, "\x1b[>0;")) {
			is_kitty = 1;
		} else if (matches_da_prefix(da_buffer, da_length, "\x1b[>7;")) {
			is_ghostty = 1;
		}
	}
	if (is_terminal_app) {
		caps = make_terminal_app_capabilities(has_truecolor);
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	if (is_warp) {
		caps = make_warp_capabilities(has_truecolor);
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	if (is_iterm) {
		caps = make_iterm_capabilities(has_truecolor);
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	if (is_vte) {
		caps = make_vte_capabilities(has_truecolor);
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	if (is_wezterm) {
		caps = make_wezterm_capabilities(has_truecolor);
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	if (is_kitty) {
		caps = make_kitty_capabilities(has_truecolor);
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	if (is_ghostty) {
		caps = make_ghostty_capabilities(has_truecolor);
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	clamp_capabilities_to_request(&caps, requested_profile);
	return caps;
}

terse_capabilities_t
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
		.mouse = TERSE_MOUSE_NONE,
		.has_bracketed_paste = 0,
		.has_title = 0,
		.has_hyperlinks = 0,
		.has_cursor_shape = 0,
		.colors = TERSE_COLOR_NONE,
		.effects = 0,
		.has_clipboard_write = 0,
		.images = TERSE_IMAGE_NONE,
		.notifications = 0,
	};
	return caps;
}