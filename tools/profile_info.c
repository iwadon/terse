#include "terse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__human68k__) && !defined(__HUMAN68K__)
#include <unistd.h>
#else
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#endif

static const char *profile_name(terse_profile_t profile)
{
	switch (profile) {
	case TERSE_P0:
		return "P0 (Basic output)";
	case TERSE_P1:
		return "P1 (Colors and styles)";
	case TERSE_P2:
		return "P2 (Advanced I/O)";
	case TERSE_P3:
		return "P3 (Extended features)";
	case TERSE_PROFILE_AUTO:
		return "AUTO (detect)";
	default:
		return "UNKNOWN";
	}
}

static const char *color_support_name(terse_color_support_t colors)
{
	switch (colors) {
	case TERSE_COLOR_NONE:
		return "None";
	case TERSE_COLOR_BASIC16:
		return "Basic 16 colors";
	case TERSE_COLOR_PALETTE256:
		return "256-color palette";
	case TERSE_COLOR_TRUECOLOR:
		return "TrueColor (16M colors)";
	default:
		return "Unknown";
	}
}

static const char *mouse_mode_name(terse_mouse_mode_t mode)
{
	switch (mode) {
	case TERSE_MOUSE_NONE:
		return "None";
	case TERSE_MOUSE_X10:
		return "X10";
	case TERSE_MOUSE_VT200:
		return "VT200";
	case TERSE_MOUSE_SGR:
		return "SGR";
	default:
		return "Unknown";
	}
}

static const char *image_support_name(terse_image_support_t images)
{
	switch (images) {
	case TERSE_IMAGE_NONE:
		return "None";
	case TERSE_IMAGE_ITERM_INLINE:
		return "iTerm2 inline";
	case TERSE_IMAGE_SIXEL:
		return "Sixel";
	case TERSE_IMAGE_KITTY:
		return "Kitty graphics";
	default:
		return "Unknown";
	}
}

static void print_yes_no(const char *label, int value)
{
	printf("  %-30s : %s\n", label, value ? "yes" : "no");
}

static void print_notification_support(unsigned int notifications)
{
	printf("  Notification support           : ");
	if (notifications == 0) {
		printf("None\n");
		return;
	}
	int first = 1;
	if (notifications & TERSE_NOTIFICATION_SUPPORT_BELL) {
		printf("%sBell", first ? "" : ", ");
		first = 0;
	}
	if (notifications & TERSE_NOTIFICATION_SUPPORT_VISUAL) {
		printf("%sVisual", first ? "" : ", ");
		first = 0;
	}
	if (notifications & TERSE_NOTIFICATION_SUPPORT_DESKTOP) {
		printf("%sDesktop", first ? "" : ", ");
	}
	printf("\n");
}

static void print_keyboard_features(unsigned int features)
{
	printf("  Keyboard features              : ");
	if (features == 0) {
		printf("None\n");
	} else {
		printf("0x%x\n", features);
	}
}

static void print_text_effects(unsigned int effects)
{
	printf("  Text effects                   : ");
	if (effects == 0) {
		printf("None\n");
		return;
	}
	int first = 1;
	if (effects & TERSE_STYLE_BOLD) {
		printf("%sBold", first ? "" : ", ");
		first = 0;
	}
	if (effects & TERSE_STYLE_FAINT) {
		printf("%sFaint", first ? "" : ", ");
		first = 0;
	}
	if (effects & TERSE_STYLE_ITALIC) {
		printf("%sItalic", first ? "" : ", ");
		first = 0;
	}
	if (effects & TERSE_STYLE_UNDERLINE) {
		printf("%sUnderline", first ? "" : ", ");
		first = 0;
	}
	if (effects & TERSE_STYLE_BLINK) {
		printf("%sBlink", first ? "" : ", ");
		first = 0;
	}
	if (effects & TERSE_STYLE_INVERSE) {
		printf("%sInverse", first ? "" : ", ");
		first = 0;
	}
	if (effects & TERSE_STYLE_STRIKE) {
		printf("%sStrike", first ? "" : ", ");
	}
	printf("\n");
}

static void print_capabilities(terse_handle_t handle)
{
	terse_capabilities_t caps = terse_get_capabilities(handle);

	printf("\n=== Detected Profile ===\n");
	printf("Profile: %s\n", profile_name(caps.profile));

	printf("\n=== P0 Features (Basic output) ===\n");
	print_yes_no("Basic output", caps.has_basic_output);
	print_yes_no("Cursor visibility", caps.has_cursor_visibility);
	print_yes_no("Move absolute", caps.has_move_absolute);
	print_yes_no("Move relative", caps.has_move_relative);
	print_yes_no("Clear line", caps.has_clear_line);
	print_yes_no("Clear screen", caps.has_clear_screen);
	print_yes_no("Terminal size detection", caps.has_size);

	printf("\n=== P1 Features (Colors and styles) ===\n");
	printf("  Color support                  : %s\n", color_support_name(caps.colors));
	print_yes_no("Basic SGR", caps.has_sgr_basic);
	print_yes_no("Extended SGR", caps.has_sgr_extended);
	print_yes_no("TrueColor", caps.has_truecolor);
	print_yes_no("Text styles", caps.has_text_styles);
	print_text_effects(caps.effects);

	printf("\n=== P2 Features (Advanced I/O) ===\n");
	printf("  Mouse mode                     : %s\n", mouse_mode_name(caps.mouse));
	print_yes_no("Bracketed paste", caps.has_bracketed_paste);
	print_yes_no("Window title", caps.has_title);
	print_yes_no("Hyperlinks", caps.has_hyperlinks);
	print_keyboard_features(caps.keyboard_features);

	printf("\n=== P3 Features (Extended features) ===\n");
	print_yes_no("Cursor shape", caps.has_cursor_shape);
	print_yes_no("Clipboard write", caps.has_clipboard_write);
	printf("  Image support                  : %s\n", image_support_name(caps.images));
	print_notification_support(caps.notifications);
}

static void print_terminal_info(void)
{
	printf("\n=== Terminal Environment ===\n");
	const char *term = getenv("TERM");
	const char *term_program = getenv("TERM_PROGRAM");
	const char *colorterm = getenv("COLORTERM");

	printf("  TERM                           : %s\n", term ? term : "(unset)");
	printf("  TERM_PROGRAM                   : %s\n", term_program ? term_program : "(unset)");
	printf("  COLORTERM                      : %s\n", colorterm ? colorterm : "(unset)");

#if !defined(__human68k__) && !defined(__HUMAN68K__)
	const char *vte_version = getenv("VTE_VERSION");
	const char *iterm_session = getenv("ITERM_SESSION_ID");
	const char *wezterm = getenv("WEZTERM_EXECUTABLE");
	const char *kitty = getenv("KITTY_PID");

	if (vte_version) {
		printf("  VTE_VERSION                    : %s\n", vte_version);
	}
	if (iterm_session) {
		printf("  ITERM_SESSION_ID               : %s\n", iterm_session);
	}
	if (wezterm) {
		printf("  WEZTERM_EXECUTABLE             : %s\n", wezterm);
	}
	if (kitty) {
		printf("  KITTY_PID                      : %s\n", kitty);
	}
#endif
}

static void print_terminal_size(terse_handle_t handle)
{
	terse_size_t size = terse_get_size(handle);
	if (size.known) {
		printf("\n=== Terminal Size ===\n");
		printf("  Rows: %d, Columns: %d\n", size.rows, size.cols);
	}
}

int main(int argc, char *argv[])
{
	terse_profile_t profile = TERSE_PROFILE_AUTO;

	// Parse command line arguments
	if (argc > 1) {
		const char *arg = argv[1];
		if (strcmp(arg, "P0") == 0 || strcmp(arg, "p0") == 0) {
			profile = TERSE_P0;
		} else if (strcmp(arg, "P1") == 0 || strcmp(arg, "p1") == 0) {
			profile = TERSE_P1;
		} else if (strcmp(arg, "P2") == 0 || strcmp(arg, "p2") == 0) {
			profile = TERSE_P2;
		} else if (strcmp(arg, "P3") == 0 || strcmp(arg, "p3") == 0) {
			profile = TERSE_P3;
		} else if (strcmp(arg, "AUTO") == 0 || strcmp(arg, "auto") == 0) {
			profile = TERSE_PROFILE_AUTO;
		} else {
			fprintf(stderr, "Usage: %s [P0|P1|P2|P3|AUTO]\n", argv[0]);
			fprintf(stderr, "Default: AUTO\n");
			return 1;
		}
	}

	printf("=== Terse Profile Information ===\n");
	printf("Requested profile: %s\n", profile_name(profile));

	print_terminal_info();

	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0
	};

	terse_handle_t handle = terse_open(profile, &options);
	if (!handle) {
		fprintf(stderr, "\nERROR: terse_open failed\n");
		return 1;
	}

	print_capabilities(handle);
	print_terminal_size(handle);

	printf("\n");
	terse_close(handle);
	return 0;
}
