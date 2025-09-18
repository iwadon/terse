#include "terse.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

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
	unsetenv("TERM_PROGRAM");
	unsetenv("TERM_PROGRAM_VERSION");
	unsetenv("LC_TERMINAL");
	unsetenv("LC_TERMINAL_VERSION");
	unsetenv("COLORTERM");
	unsetenv("GNOME_TERMINAL_SCREEN");
	unsetenv("GNOME_TERMINAL_SERVICE");
	unsetenv("VTE_VERSION");
	unsetenv("TERSE_SECONDARY_DA_HINT");
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

TEST(TerseOpen, ReturnsP0Profile_OnRequestAboveP0)
{
	env_backup_t term_program;
	env_backup_t lc_terminal;
	env_backup_t colorterm;
	env_backup_t gnome_screen;
	env_backup_t gnome_service;
	env_backup_t vte_version;
	env_backup_t secondary_da;
	backup_env(&term_program, "TERM_PROGRAM");
	backup_env(&lc_terminal, "LC_TERMINAL");
	backup_env(&colorterm, "COLORTERM");
	backup_env(&gnome_screen, "GNOME_TERMINAL_SCREEN");
	backup_env(&gnome_service, "GNOME_TERMINAL_SERVICE");
	backup_env(&vte_version, "VTE_VERSION");
	backup_env(&secondary_da, "TERSE_SECONDARY_DA_HINT");
	clear_detection_environment();
	terse_handle_t handle = terse_open(TERSE_P3, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P0);
	terse_close(handle);
	restore_env(&term_program);
	restore_env(&lc_terminal);
	restore_env(&colorterm);
	restore_env(&gnome_screen);
	restore_env(&gnome_service);
	restore_env(&vte_version);
	restore_env(&secondary_da);
}

TEST(TerseOpen, DetectsP1Profile_OnAppleTerminalEnv)
{
	env_backup_t term_program;
	env_backup_t lc_terminal;
	env_backup_t colorterm;
	env_backup_t secondary_da;
	backup_env(&term_program, "TERM_PROGRAM");
	backup_env(&lc_terminal, "LC_TERMINAL");
	backup_env(&colorterm, "COLORTERM");
	backup_env(&secondary_da, "TERSE_SECONDARY_DA_HINT");
	clear_detection_environment();
	setenv("TERM_PROGRAM", "Apple_Terminal", 1);
	setenv("LC_TERMINAL", "Apple_Terminal", 1);
	setenv("COLORTERM", "truecolor", 1);
	terse_handle_t handle = terse_open(TERSE_P3, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P1);
	EXPECT_EQ(caps.has_truecolor, 1);
	EXPECT_EQ(caps.has_bracketed_paste, 0);
	terse_close(handle);
	restore_env(&term_program);
	restore_env(&lc_terminal);
	restore_env(&colorterm);
	restore_env(&secondary_da);
}

TEST(TerseOpen, DetectsP2Profile_OnVteSignatures)
{
	env_backup_t term_program;
	env_backup_t lc_terminal;
	env_backup_t colorterm;
	env_backup_t gnome_screen;
	env_backup_t gnome_service;
	env_backup_t vte_version;
	env_backup_t secondary_da;
	backup_env(&term_program, "TERM_PROGRAM");
	backup_env(&lc_terminal, "LC_TERMINAL");
	backup_env(&colorterm, "COLORTERM");
	backup_env(&gnome_screen, "GNOME_TERMINAL_SCREEN");
	backup_env(&gnome_service, "GNOME_TERMINAL_SERVICE");
	backup_env(&vte_version, "VTE_VERSION");
	backup_env(&secondary_da, "TERSE_SECONDARY_DA_HINT");
	clear_detection_environment();
	setenv("COLORTERM", "truecolor", 1);
	setenv("TERSE_SECONDARY_DA_HINT", "\x1b[>61;7800;1c", 1);
	terse_handle_t handle = terse_open(TERSE_P3, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P2);
	EXPECT_EQ(caps.mouse, TERSE_MOUSE_SGR);
	EXPECT_EQ(caps.has_bracketed_paste, 1);
	terse_close(handle);
	restore_env(&term_program);
	restore_env(&lc_terminal);
	restore_env(&colorterm);
	restore_env(&gnome_screen);
	restore_env(&gnome_service);
	restore_env(&vte_version);
	restore_env(&secondary_da);
}

int main()
{
	return RunAllTests();
}
