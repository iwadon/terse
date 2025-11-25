/**
 * @file terse_detection.c
 * @brief Terminal capability detection implementation.
 *
 * This module detects terminal capabilities by examining environment variables
 * and probing with Secondary Device Attributes (DA) sequences. It supports
 * automatic identification of Apple Terminal, GNOME Terminal/VTE, iTerm2,
 * Windows Terminal, WezTerm, kitty, Ghostty, Warp, and Sixel-capable terminals.
 */

#include "terse_detection.h"
#include "terse_platform.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

/* Forward declarations for internal functions */
static int matches_da_prefix(const unsigned char *buffer, size_t length, const char *prefix);
static terse_capabilities_t make_terminal_app_capabilities(int has_truecolor);
static terse_capabilities_t make_vte_capabilities(int has_truecolor);
static terse_capabilities_t make_iterm_capabilities(int has_truecolor);
static terse_capabilities_t make_wezterm_capabilities(int has_truecolor);
static terse_capabilities_t make_kitty_capabilities(int has_truecolor);
static terse_capabilities_t make_ghostty_capabilities(int has_truecolor);
static terse_capabilities_t make_sixel_capabilities(int has_truecolor);
static terse_capabilities_t make_warp_capabilities(int has_truecolor);
static terse_capabilities_t make_windows_terminal_capabilities(int has_truecolor);
static void clamp_capabilities_to_request(terse_capabilities_t *caps, terse_profile_t requested);
static int is_multiplexer_session(const char *term);
static int term_supports_sixel(const char *term);

/**
 * Create a baseline P0 capabilities structure.
 *
 * P0 provides only basic output: cursor movement, screen/line clearing,
 * and text output. No colors, mouse, or advanced features.
 *
 * @return A capabilities structure initialized to P0 level.
 */
terse_capabilities_t
terse_make_p0_capabilities(void)
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
		.keyboard_features = 0,
	};
	return caps;
}

/**
 * Return a numeric rank for a color kind (for comparison purposes).
 *
 * @param kind The color kind to rank.
 * @return An integer rank (0=default, 1=basic16, 2=palette256, 3=truecolor).
 */
int terse_color_kind_rank(terse_color_kind_t kind)
{
	switch (kind) {
	case TERSE_COLOR_KIND_DEFAULT:
		return 0;
	case TERSE_COLOR_KIND_BASIC16:
		return 1;
	case TERSE_COLOR_KIND_PALETTE256:
		return 2;
	case TERSE_COLOR_KIND_TRUECOLOR:
		return 3;
	default:
		return 0;
	}
}

/**
 * Check if a device attributes buffer matches a given prefix string.
 *
 * @param buffer The buffer containing the DA response.
 * @param length The length of the buffer.
 * @param prefix The expected prefix string.
 * @return 1 if the prefix matches, 0 otherwise.
 */
static int
matches_da_prefix(const unsigned char *buffer, size_t length, const char *prefix)
{
	if (!buffer || length == 0 || !prefix) {
		return 0;
	}
	size_t prefix_len = strlen(prefix);
	if (prefix_len == 0 || length < prefix_len) {
		return 0;
	}
	return memcmp(buffer, prefix, prefix_len) == 0;
}

/**
 * Create capabilities for Apple Terminal.
 *
 * @param has_truecolor Whether the terminal supports TrueColor.
 * @return A P1 capabilities structure for Terminal.app.
 */
static terse_capabilities_t
make_terminal_app_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = terse_make_p0_capabilities();
	caps.profile = TERSE_P1;
	caps.has_sgr_basic = 1;
	caps.has_sgr_extended = 1;
	caps.has_truecolor = has_truecolor ? 1 : 0;
	caps.has_text_styles = 1;
	caps.has_title = 1;
	caps.notifications |= TERSE_NOTIFICATION_SUPPORT_BELL;
	return caps;
}

/**
 * Create capabilities for VTE-based terminals (GNOME Terminal, Tilix, etc.).
 *
 * @param has_truecolor Whether the terminal supports TrueColor.
 * @return A P2 capabilities structure for VTE terminals.
 */
static terse_capabilities_t
make_vte_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_terminal_app_capabilities(has_truecolor);
	caps.profile = TERSE_P2;
	caps.mouse = TERSE_MOUSE_SGR;
	caps.has_bracketed_paste = 1;
	caps.has_hyperlinks = 1;
	caps.has_cursor_shape = 1;
	return caps;
}

/**
 * Create capabilities for iTerm2.
 *
 * @param has_truecolor Whether the terminal supports TrueColor.
 * @return A P3 capabilities structure for iTerm2.
 */
static terse_capabilities_t
make_iterm_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_vte_capabilities(has_truecolor);
	caps.profile = TERSE_P3;
	caps.has_clipboard_write = 1;
	caps.images = TERSE_IMAGE_ITERM_INLINE;
	caps.notifications |= TERSE_NOTIFICATION_SUPPORT_DESKTOP;
	return caps;
}

/**
 * Create capabilities for WezTerm.
 *
 * @param has_truecolor Whether the terminal supports TrueColor.
 * @return A P3 capabilities structure for WezTerm.
 */
static terse_capabilities_t
make_wezterm_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_vte_capabilities(has_truecolor);
	caps.profile = TERSE_P3;
	caps.has_clipboard_write = 1;
	caps.images = TERSE_IMAGE_KITTY;
	caps.notifications |= TERSE_NOTIFICATION_SUPPORT_DESKTOP;
	return caps;
}

/**
 * Create capabilities for kitty terminal.
 *
 * @param has_truecolor Whether the terminal supports TrueColor.
 * @return A P3 capabilities structure for kitty.
 */
static terse_capabilities_t
make_kitty_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_vte_capabilities(has_truecolor);
	caps.profile = TERSE_P3;
	caps.has_clipboard_write = 1;
	caps.images = TERSE_IMAGE_KITTY;
	return caps;
}

/**
 * Create capabilities for Ghostty terminal.
 *
 * @param has_truecolor Whether the terminal supports TrueColor.
 * @return A P3 capabilities structure for Ghostty.
 */
static terse_capabilities_t
make_ghostty_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_vte_capabilities(has_truecolor);
	caps.profile = TERSE_P3;
	caps.has_clipboard_write = 1;
	return caps;
}

/**
 * Create capabilities for Sixel-capable terminals.
 *
 * @param has_truecolor Whether the terminal supports TrueColor.
 * @return A P3 capabilities structure with Sixel support.
 */
static terse_capabilities_t
make_sixel_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_terminal_app_capabilities(has_truecolor);
	caps.profile = TERSE_P3;
	caps.images = TERSE_IMAGE_SIXEL;
	return caps;
}

/**
 * Create capabilities for Warp terminal.
 *
 * @param has_truecolor Whether the terminal supports TrueColor.
 * @return A P1 capabilities structure for Warp.
 */
static terse_capabilities_t
make_warp_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_terminal_app_capabilities(has_truecolor);
	caps.profile = TERSE_P1;
	return caps;
}

/**
 * Create capabilities for Windows Terminal.
 *
 * Windows Terminal (Microsoft Terminal) supports advanced features including
 * TrueColor, mouse, bracketed paste, hyperlinks, cursor shapes, and Sixel images.
 * Recent versions also have experimental support for the Kitty keyboard protocol.
 *
 * @param has_truecolor Whether the terminal supports TrueColor (usually yes).
 * @return A P3 capabilities structure for Windows Terminal.
 */
static terse_capabilities_t
make_windows_terminal_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_vte_capabilities(has_truecolor);
	caps.profile = TERSE_P3;
	caps.images = TERSE_IMAGE_SIXEL;
	caps.has_cursor_shape = 1;
	return caps;
}

/**
 * Clamp capabilities to the requested profile level.
 *
 * If the user requested a specific profile (P0, P1, P2), remove features
 * that exceed that profile level. TERSE_PROFILE_AUTO is left untouched.
 *
 * @param caps Pointer to the capabilities structure to modify.
 * @param requested The profile level requested by the user.
 */
static void
clamp_capabilities_to_request(terse_capabilities_t *caps, terse_profile_t requested)
{
	if (!caps) {
		return;
	}
	if (requested == TERSE_PROFILE_AUTO) {
		return;
	}
	if (requested <= TERSE_P0) {
		*caps = terse_make_p0_capabilities();
		return;
	}
	if (requested == TERSE_P1 && caps->profile > TERSE_P1) {
		caps->profile = TERSE_P1;
		caps->mouse = TERSE_MOUSE_NONE;
		caps->has_bracketed_paste = 0;
		caps->has_hyperlinks = 0;
		caps->has_clipboard_write = 0;
		caps->images = TERSE_IMAGE_NONE;
		caps->notifications &= TERSE_NOTIFICATION_SUPPORT_BELL;
	}
	if (requested == TERSE_P2 && caps->profile > TERSE_P2) {
		caps->profile = TERSE_P2;
		caps->images = TERSE_IMAGE_NONE;
		caps->notifications &= ~(TERSE_NOTIFICATION_SUPPORT_DESKTOP);
	}
}

/**
 * Check if running inside a terminal multiplexer (tmux or screen).
 *
 * Multiplexers often disable advanced features like inline images.
 *
 * @param term The value of the TERM environment variable.
 * @return 1 if in a multiplexer session, 0 otherwise.
 */
static int
is_multiplexer_session(const char *term)
{
	if (getenv("TMUX")) {
		return 1;
	}
	if (!term) {
		return 0;
	}
	if (strstr(term, "tmux") || strstr(term, "screen")) {
		return 1;
	}
	return 0;
}

/**
 * Check if the TERM variable indicates Sixel support.
 *
 * @param term The value of the TERM environment variable.
 * @return 1 if Sixel is supported, 0 otherwise.
 */
static int
term_supports_sixel(const char *term)
{
	if (!term) {
		return 0;
	}
	if (strstr(term, "sixel")) {
		return 1;
	}
	if (strcmp(term, "mlterm") == 0 || strcmp(term, "mlterm-direct") == 0) {
		return 1;
	}
	if (strcmp(term, "contour") == 0) {
		return 1;
	}
	if (strcmp(term, "yaft-256color") == 0 || strcmp(term, "yaft-sixel") == 0) {
		return 1;
	}
	return 0;
}

/**
 * Detect terminal capabilities based on environment and DA probing.
 *
 * This is the main entry point for capability detection. It examines
 * environment variables (TERM, TERM_PROGRAM, COLORTERM, etc.) and
 * optionally probes the terminal with Secondary Device Attributes.
 *
 * Detection priority (from highest to lowest):
 * 1. Apple Terminal (TERM_PROGRAM=Apple_Terminal or DA prefix)
 * 2. Warp Terminal (TERM_PROGRAM=WarpTerminal or DA prefix)
 * 3. iTerm2 (TERM_PROGRAM=iTerm.app or DA prefix)
 * 4. VTE-based terminals (GNOME_TERMINAL_SCREEN, VTE_VERSION, or DA prefix)
 * 5. Windows Terminal (WT_SESSION or DA prefix \x1b[>0;10;)
 * 6. WezTerm (TERM_PROGRAM=WezTerm or DA prefix)
 * 7. kitty (TERM=xterm-kitty, KITTY_PID, or DA prefix)
 * 8. Ghostty (TERM=xterm-ghostty or DA prefix)
 * 9. Sixel-capable terminals (mlterm, contour, yaft, etc.)
 * 10. Fallback to P0 (basic output only)
 *
 * @param requested_profile The profile level requested by the user.
 * @param options The options structure containing file descriptors for probing.
 * @return A capabilities structure with detected features.
 */
terse_capabilities_t
detect_environment_capabilities(terse_profile_t requested_profile, const terse_options_t *options)
{
	terse_capabilities_t caps = terse_make_p0_capabilities();
	int auto_requested = requested_profile == TERSE_PROFILE_AUTO;
	if (!auto_requested && requested_profile == TERSE_P0) {
		return caps;
	}
	const char *term = getenv("TERM");
	const char *term_program = getenv("TERM_PROGRAM");
	const char *lc_terminal = getenv("LC_TERMINAL");
	const char *colorterm = getenv("COLORTERM");
	const char *gnome_screen = getenv("GNOME_TERMINAL_SCREEN");
	const char *gnome_service = getenv("GNOME_TERMINAL_SERVICE");
	const char *vte_version = getenv("VTE_VERSION");
	const char *secondary_hint = getenv("TERSE_SECONDARY_DA_HINT");
	unsigned char secondary[128];
	memset(secondary, 0, sizeof(secondary));
	size_t secondary_len = 0;
	if (secondary_hint && *secondary_hint) {
		size_t hint_len = strlen(secondary_hint);
		if (hint_len > sizeof(secondary)) {
			hint_len = sizeof(secondary);
		}
		memcpy(secondary, secondary_hint, hint_len);
		secondary_len = hint_len;
	} else if (options) {
		secondary_len = terse_platform_probe_secondary_da(options->input_fd, options->output_fd, secondary, sizeof(secondary));
	}
	int has_truecolor = (colorterm && strcasecmp(colorterm, "truecolor") == 0) ? 1 : 0;
	int is_mux = is_multiplexer_session(term);
	int is_terminal_app = 0;
	if (term_program && strcmp(term_program, "Apple_Terminal") == 0) {
		is_terminal_app = 1;
	}
	if (!is_terminal_app && lc_terminal && strcmp(lc_terminal, "Apple_Terminal") == 0) {
		is_terminal_app = 1;
	}
	if (!is_terminal_app && matches_da_prefix(secondary, secondary_len, "\x1b[>1;95;0c")) {
		is_terminal_app = 1;
	}
	if (is_terminal_app) {
		caps = make_terminal_app_capabilities(has_truecolor);
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	int is_warp = 0;
	if (term_program && strcmp(term_program, "WarpTerminal") == 0) {
		is_warp = 1;
	}
	if (!is_warp && matches_da_prefix(secondary, secondary_len, "\x1b[>0;95;0c")) {
		is_warp = 1;
	}
	if (is_warp) {
		caps = make_warp_capabilities(has_truecolor);
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	int is_iterm = 0;
	if (term_program && strcmp(term_program, "iTerm.app") == 0) {
		is_iterm = 1;
	}
	if (!is_iterm && lc_terminal && strcmp(lc_terminal, "iTerm2") == 0) {
		is_iterm = 1;
	}
	if (!is_iterm && matches_da_prefix(secondary, secondary_len, "\x1b[>64;")) {
		is_iterm = 1;
	}
	if (is_iterm) {
		caps = make_iterm_capabilities(has_truecolor);
		caps.keyboard_features |= TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL;
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	int is_vte = 0;
	if ((gnome_screen && *gnome_screen) || (gnome_service && *gnome_service) || (vte_version && *vte_version)) {
		is_vte = 1;
	}
	if (!is_vte && matches_da_prefix(secondary, secondary_len, "\x1b[>61;")) {
		is_vte = 1;
	}
	if (!is_vte && matches_da_prefix(secondary, secondary_len, "\x1b[>65;")) {
		is_vte = 1;
	}
	if (is_vte) {
		caps = make_vte_capabilities(has_truecolor);
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	int is_windows_terminal = 0;
	const char *wt_session = getenv("WT_SESSION");
	if (wt_session && *wt_session) {
		is_windows_terminal = 1;
	}
	if (!is_windows_terminal && matches_da_prefix(secondary, secondary_len, "\x1b[>0;10;")) {
		is_windows_terminal = 1;
	}
	if (is_windows_terminal) {
		caps = make_windows_terminal_capabilities(has_truecolor);
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	int is_wezterm = 0;
	if (term_program && strcmp(term_program, "WezTerm") == 0) {
		is_wezterm = 1;
	}
	if (!is_wezterm) {
		const char *wezexec = getenv("WEZTERM_EXECUTABLE");
		if (wezexec && *wezexec) {
			is_wezterm = 1;
		}
	}
	if (!is_wezterm && matches_da_prefix(secondary, secondary_len, "\x1b[>1;277;")) {
		is_wezterm = 1;
	}
	if (is_wezterm) {
		caps = make_wezterm_capabilities(has_truecolor);
		caps.keyboard_features |= TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS;
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	int is_kitty = 0;
	if (term && strcmp(term, "xterm-kitty") == 0) {
		is_kitty = 1;
	}
	if (!is_kitty) {
		const char *kitty_pid = getenv("KITTY_PID");
		if (kitty_pid && *kitty_pid) {
			is_kitty = 1;
		}
	}
	if (!is_kitty && matches_da_prefix(secondary, secondary_len, "\x1b[>1;4000;")) {
		is_kitty = 1;
	}
	if (is_kitty) {
		caps = make_kitty_capabilities(has_truecolor);
		caps.keyboard_features |= TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL;
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	int is_ghostty = 0;
	if (term && strcmp(term, "xterm-ghostty") == 0) {
		is_ghostty = 1;
	}
	if (!is_ghostty && term_program && strcmp(term_program, "ghostty") == 0) {
		is_ghostty = 1;
	}
	if (!is_ghostty && matches_da_prefix(secondary, secondary_len, "\x1b[>1;10;")) {
		is_ghostty = 1;
	}
	if (is_ghostty) {
		caps = make_ghostty_capabilities(has_truecolor);
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	if (term_supports_sixel(term)) {
		caps = make_sixel_capabilities(has_truecolor);
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	clamp_capabilities_to_request(&caps, requested_profile);
	return caps;
}
