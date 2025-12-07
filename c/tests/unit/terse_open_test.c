#include "terse.h"
#include <attest/attest.h>

#include <stdlib.h>
#include <string.h>

#include "test_compat.h"

#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

typedef struct env_backup {
	const char *name;
	char *value;
	int had_value;
} env_backup_t;

static void
backup_env(env_backup_t *backup, const char *name)
{
	backup->name = name;
	backup->value = NULL;
	backup->had_value = 0;
	const char *current = getenv(name);
	if (current) {
		char *copy = strdup(current);
		if (copy) {
			backup->value = copy;
			backup->had_value = 1;
		}
	}
}

static void
restore_env(env_backup_t *backup)
{
	if (backup->had_value) {
		setenv(backup->name, backup->value, 1);
	} else {
		unsetenv(backup->name);
	}
	free(backup->value);
	backup->value = NULL;
	backup->had_value = 0;
}

static void
clear_detection_environment(void)
{
	unsetenv("TERM");
	unsetenv("TERM_PROGRAM");
	unsetenv("TERM_PROGRAM_VERSION");
	unsetenv("LC_TERMINAL");
	unsetenv("LC_TERMINAL_VERSION");
	unsetenv("COLORTERM");
	unsetenv("GNOME_TERMINAL_SCREEN");
	unsetenv("GNOME_TERMINAL_SERVICE");
	unsetenv("VTE_VERSION");
	unsetenv("TERSE_SECONDARY_DA_HINT");
	unsetenv("WEZTERM_EXECUTABLE");
	unsetenv("KITTY_PID");
	unsetenv("WT_SESSION");
}

static void
backup_env_list(env_backup_t *backups, size_t count, const char *const *names)
{
	for (size_t i = 0; i < count; ++i) {
		backup_env(&backups[i], names[i]);
	}
}

static void
restore_env_list(env_backup_t *backups, size_t count)
{
	for (size_t i = 0; i < count; ++i) {
		restore_env(&backups[i]);
	}
}

TEST(TerseOpen, ReturnsNonNull_OnValidProfile)
{
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_close(handle);
}

TEST(TerseOpen, ReturnsNull_OnInvalidProfile)
{
	terse_handle_t handle = terse_open((terse_profile_t)999, NULL);
	EXPECT_TRUE(handle == NULL);
}

TEST(TerseOpen, ReturnsP0Profile_OnExplicitP3WithoutHints)
{
	static const char *const names[] = {
		"TERM",
		"TERM_PROGRAM",
		"TERM_PROGRAM_VERSION",
		"LC_TERMINAL",
		"LC_TERMINAL_VERSION",
		"COLORTERM",
		"GNOME_TERMINAL_SCREEN",
		"GNOME_TERMINAL_SERVICE",
		"VTE_VERSION",
		"TERSE_SECONDARY_DA_HINT",
		"WEZTERM_EXECUTABLE",
		"KITTY_PID",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	terse_handle_t handle = terse_open(TERSE_P3, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P0);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseOpen, ReturnsP0Profile_OnAutoWithoutHints)
{
	static const char *const names[] = {
		"TERM",
		"TERM_PROGRAM",
		"TERM_PROGRAM_VERSION",
		"LC_TERMINAL",
		"LC_TERMINAL_VERSION",
		"COLORTERM",
		"GNOME_TERMINAL_SCREEN",
		"GNOME_TERMINAL_SERVICE",
		"VTE_VERSION",
		"TERSE_SECONDARY_DA_HINT",
		"WEZTERM_EXECUTABLE",
		"KITTY_PID",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P0);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseOpen, DetectsP1Profile_OnAppleTerminalEnv)
{
	static const char *const names[] = {
		"TERM",
		"TERM_PROGRAM",
		"LC_TERMINAL",
		"COLORTERM",
		"TERSE_SECONDARY_DA_HINT",
		"KITTY_PID",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	setenv("TERM_PROGRAM", "Apple_Terminal", 1);
	setenv("LC_TERMINAL", "Apple_Terminal", 1);
	setenv("COLORTERM", "truecolor", 1);
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P1);
	EXPECT_EQ(caps.has_truecolor, 1);
	EXPECT_EQ(caps.has_bracketed_paste, 0);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseOpen, DetectsP1Profile_OnWarpTerminal)
{
	static const char *const names[] = {
		"TERM",
		"TERM_PROGRAM",
		"TERM_PROGRAM_VERSION",
		"COLORTERM",
		"TERSE_SECONDARY_DA_HINT",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	setenv("TERM", "xterm-256color", 1);
	setenv("TERM_PROGRAM", "WarpTerminal", 1);
	setenv("TERM_PROGRAM_VERSION", "v0.2025.09.10.08.11.stable_01", 1);
	setenv("COLORTERM", "truecolor", 1);
	setenv("TERSE_SECONDARY_DA_HINT", "\x1b[>0;95;0c", 1);
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P1);
	EXPECT_EQ(caps.has_truecolor, 1);
	EXPECT_EQ(caps.has_bracketed_paste, 0);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseOpen, DetectsP2Profile_OnVteSignatures)
{
	static const char *const names[] = {
		"TERM",
		"TERM_PROGRAM",
		"LC_TERMINAL",
		"COLORTERM",
		"GNOME_TERMINAL_SCREEN",
		"GNOME_TERMINAL_SERVICE",
		"VTE_VERSION",
		"TERSE_SECONDARY_DA_HINT",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	setenv("COLORTERM", "truecolor", 1);
	setenv("TERSE_SECONDARY_DA_HINT", "\x1b[>61;7800;1c", 1);
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P2);
	EXPECT_EQ(caps.mouse, TERSE_MOUSE_SGR);
	EXPECT_EQ(caps.has_bracketed_paste, 1);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseOpen, DetectsP3Profile_OnITermEnv)
{
	static const char *const names[] = {
		"TERM",
		"TERM_PROGRAM",
		"TERM_PROGRAM_VERSION",
		"LC_TERMINAL",
		"COLORTERM",
		"TERSE_SECONDARY_DA_HINT",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	setenv("TERM", "xterm-256color", 1);
	setenv("TERM_PROGRAM", "iTerm.app", 1);
	setenv("TERM_PROGRAM_VERSION", "3.5.14", 1);
	setenv("LC_TERMINAL", "iTerm2", 1);
	setenv("COLORTERM", "truecolor", 1);
	setenv("TERSE_SECONDARY_DA_HINT", "\x1b[>64;2500;0c", 1);
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P3);
	EXPECT_EQ(caps.images, TERSE_IMAGE_ITERM_INLINE);
	EXPECT_EQ(caps.has_clipboard_write, 1);
	EXPECT_EQ(caps.has_bracketed_paste, 1);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseOpen, DetectsP3Profile_OnWezTermEnv)
{
	static const char *const names[] = {
		"TERM",
		"TERM_PROGRAM",
		"TERM_PROGRAM_VERSION",
		"COLORTERM",
		"WEZTERM_EXECUTABLE",
		"TERSE_SECONDARY_DA_HINT",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	setenv("TERM", "xterm-256color", 1);
	setenv("TERM_PROGRAM", "WezTerm", 1);
	setenv("TERM_PROGRAM_VERSION", "20240203-110809-5046fc22", 1);
	setenv("COLORTERM", "truecolor", 1);
	setenv("WEZTERM_EXECUTABLE", "/Applications/WezTerm.app/Contents/MacOS/wezterm-gui", 1);
	setenv("TERSE_SECONDARY_DA_HINT", "\x1b[>1;277;0c", 1);
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P3);
	EXPECT_EQ(caps.images, TERSE_IMAGE_KITTY);
	EXPECT_EQ(caps.has_clipboard_write, 1);
	EXPECT_EQ(caps.mouse, TERSE_MOUSE_SGR);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseOpen, DetectsP3Profile_OnKittyEnv)
{
	static const char *const names[] = {
		"TERM",
		"COLORTERM",
		"KITTY_PID",
		"TERSE_SECONDARY_DA_HINT",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	setenv("TERM", "xterm-kitty", 1);
	setenv("COLORTERM", "truecolor", 1);
	setenv("KITTY_PID", "12345", 1);
	setenv("TERSE_SECONDARY_DA_HINT", "\x1b[>1;4000;42c", 1);
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P3);
	EXPECT_EQ(caps.images, TERSE_IMAGE_KITTY);
	EXPECT_EQ(caps.has_clipboard_write, 1);
	EXPECT_EQ(caps.has_bracketed_paste, 1);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseOpen, DetectsP3Profile_OnGhosttyEnv)
{
	static const char *const names[] = {
		"TERM",
		"TERM_PROGRAM",
		"TERM_PROGRAM_VERSION",
		"COLORTERM",
		"TERSE_SECONDARY_DA_HINT",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	setenv("TERM", "xterm-ghostty", 1);
	setenv("TERM_PROGRAM", "ghostty", 1);
	setenv("TERM_PROGRAM_VERSION", "1.2.0", 1);
	setenv("COLORTERM", "truecolor", 1);
	setenv("TERSE_SECONDARY_DA_HINT", "\x1b[>1;10;0c", 1);
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P3);
	EXPECT_EQ(caps.has_clipboard_write, 1);
	EXPECT_EQ(caps.has_bracketed_paste, 1);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseCapabilitiesOverride, EnablesFeaturesOnP0Baseline)
{
	static const char *const names[] = {
		"TERM",
		"TERM_PROGRAM",
		"COLORTERM",
		"TERSE_SECONDARY_DA_HINT",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P0);
	EXPECT_EQ(caps.has_sgr_basic, 0);
	EXPECT_EQ(caps.has_truecolor, 0);
	EXPECT_EQ(caps.has_text_styles, 0);
	EXPECT_EQ(caps.has_bracketed_paste, 0);
	EXPECT_EQ(terse_capabilities_enable(handle,
				  TERSE_CAP_ENABLE_SGR_BASIC | TERSE_CAP_ENABLE_TEXT_STYLES | TERSE_CAP_ENABLE_SGR_EXTENDED | TERSE_CAP_ENABLE_TRUECOLOR | TERSE_CAP_ENABLE_BRACKETED_PASTE),
		0);
	caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.has_sgr_basic, 1);
	EXPECT_EQ(caps.has_sgr_extended, 1);
	EXPECT_EQ(caps.has_truecolor, 1);
	EXPECT_EQ(caps.colors, TERSE_COLOR_TRUECOLOR);
	EXPECT_EQ(caps.has_text_styles, 1);
	EXPECT_EQ(caps.effects, TERSE_STYLE_ALL_SUPPORTED);
	EXPECT_EQ(caps.has_bracketed_paste, 1);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseCapabilitiesOverride, DisablesAndResetsOnP3Baseline)
{
	static const char *const names[] = {
		"TERM",
		"TERM_PROGRAM",
		"TERM_PROGRAM_VERSION",
		"LC_TERMINAL",
		"COLORTERM",
		"TERSE_SECONDARY_DA_HINT",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	setenv("TERM", "xterm-256color", 1);
	setenv("TERM_PROGRAM", "iTerm.app", 1);
	setenv("TERM_PROGRAM_VERSION", "3.5.14", 1);
	setenv("LC_TERMINAL", "iTerm2", 1);
	setenv("COLORTERM", "truecolor", 1);
	setenv("TERSE_SECONDARY_DA_HINT", "\x1b[>64;2500;0c", 1);
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P3);
	EXPECT_EQ(caps.images, TERSE_IMAGE_ITERM_INLINE);
	EXPECT_EQ(caps.has_clipboard_write, 1);
	EXPECT_NE(caps.notifications & TERSE_NOTIFICATION_SUPPORT_DESKTOP, 0);
	EXPECT_EQ(terse_capabilities_disable(handle,
				  TERSE_CAP_DISABLE_IMAGE_INLINE | TERSE_CAP_DISABLE_CLIPBOARD_WRITE | TERSE_CAP_DISABLE_NOTIFICATION_DESKTOP),
		0);
	caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.images, TERSE_IMAGE_NONE);
	EXPECT_EQ(caps.has_clipboard_write, 0);
	EXPECT_EQ(caps.notifications & TERSE_NOTIFICATION_SUPPORT_DESKTOP, 0u);
	EXPECT_EQ(terse_capabilities_reset_overrides(handle), 0);
	caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.images, TERSE_IMAGE_ITERM_INLINE);
	EXPECT_EQ(caps.has_clipboard_write, 1);
	EXPECT_NE(caps.notifications & TERSE_NOTIFICATION_SUPPORT_DESKTOP, 0);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseStateOverride, OverridesAndCapture)
{
	static const char *const names[] = {
		"TERM",
		"TERM_PROGRAM",
		"COLORTERM",
		"TERSE_SECONDARY_DA_HINT",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_state_t override_state = {
		.cursor_known = 1,
		.cursor_visible = 0,
		.cursor_row = 5,
		.cursor_col = 3,
		.style_known = 1,
		.style = terse_style_default(),
	};
	override_state.style.effects = TERSE_STYLE_BOLD;
	override_state.style.foreground = terse_color_basic(TERSE_BASIC_COLOR_RED, 0);
	EXPECT_EQ(terse_state_override(handle, &override_state), 0);
	terse_state_t captured;
	EXPECT_EQ(terse_capture_state(handle, &captured), 0);
	EXPECT_EQ(captured.cursor_known, 1);
	EXPECT_EQ(captured.cursor_row, 5);
	EXPECT_EQ(captured.cursor_col, 3);
	EXPECT_EQ(captured.cursor_visible, 0);
	EXPECT_EQ(captured.style_known, 1);
	EXPECT_NE(captured.style.effects & TERSE_STYLE_BOLD, 0u);
	EXPECT_EQ(captured.style.foreground.kind, TERSE_COLOR_KIND_BASIC16);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseStateOverride, ClearResetsState)
{
	static const char *const names[] = {
		"TERM",
		"TERM_PROGRAM",
		"COLORTERM",
		"TERSE_SECONDARY_DA_HINT",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_state_t override_state = {
		.cursor_known = 1,
		.cursor_visible = 0,
		.cursor_row = 7,
		.cursor_col = 2,
		.style_known = 1,
		.style = terse_style_default(),
	};
	override_state.style.effects = TERSE_STYLE_ITALIC;
	EXPECT_EQ(terse_state_override(handle, &override_state), 0);
	EXPECT_EQ(terse_state_clear(handle), 0);
	terse_state_t captured;
	EXPECT_EQ(terse_capture_state(handle, &captured), 0);
	EXPECT_EQ(captured.cursor_known, 0);
	EXPECT_EQ(captured.cursor_visible, 1);
	EXPECT_EQ(captured.style_known, 0);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseRestoreState, UpdatesWhenCapabilitiesMissing)
{
	static const char *const names[] = {
		"TERM",
		"TERM_PROGRAM",
		"COLORTERM",
		"TERSE_SECONDARY_DA_HINT",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_state_t restore_state = {
		.cursor_known = 1,
		.cursor_visible = 0,
		.cursor_row = 4,
		.cursor_col = 2,
		.style_known = 1,
		.style = terse_style_default(),
	};
	restore_state.style.effects = TERSE_STYLE_UNDERLINE;
	restore_state.style.foreground = terse_color_basic(TERSE_BASIC_COLOR_BLUE, 0);
	EXPECT_EQ(terse_restore_state(handle, &restore_state), 0);
	terse_state_t captured;
	EXPECT_EQ(terse_capture_state(handle, &captured), 0);
	EXPECT_EQ(captured.cursor_known, 1);
	EXPECT_EQ(captured.cursor_row, 4);
	EXPECT_EQ(captured.cursor_col, 2);
	EXPECT_EQ(captured.cursor_visible, 0);
	EXPECT_EQ(captured.style_known, 1);
	EXPECT_NE(captured.style.effects & TERSE_STYLE_UNDERLINE, 0u);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseOpen, DetectsP3Profile_OnWindowsTerminalEnv)
{
	static const char *const names[] = {
		"TERM",
		"WT_SESSION",
		"COLORTERM",
		"TERSE_SECONDARY_DA_HINT",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	setenv("TERM", "xterm-256color", 1);
	setenv("WT_SESSION", "7b39308d-4eee-4f17-b3dc-71fbb23be859", 1);
	setenv("COLORTERM", "truecolor", 1);
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P3);
	EXPECT_EQ(caps.images, TERSE_IMAGE_SIXEL);
	EXPECT_EQ(caps.has_bracketed_paste, 1);
	EXPECT_EQ(caps.has_hyperlinks, 1);
	EXPECT_EQ(caps.has_cursor_shape, 1);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}

TEST(TerseOpen, DetectsP3Profile_OnWindowsTerminalDA)
{
	static const char *const names[] = {
		"TERM",
		"COLORTERM",
		"TERSE_SECONDARY_DA_HINT",
	};
	env_backup_t backups[ARRAY_LEN(names)];
	backup_env_list(backups, ARRAY_LEN(names), names);
	clear_detection_environment();
	setenv("TERM", "xterm-256color", 1);
	setenv("TERSE_SECONDARY_DA_HINT", "\x1b[>0;10;1c", 1);
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P3);
	EXPECT_EQ(caps.images, TERSE_IMAGE_SIXEL);
	EXPECT_EQ(caps.has_bracketed_paste, 1);
	EXPECT_EQ(caps.has_hyperlinks, 1);
	EXPECT_EQ(caps.has_cursor_shape, 1);
	terse_close(handle);
	restore_env_list(backups, ARRAY_LEN(names));
}
