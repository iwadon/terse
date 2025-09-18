#include "terse.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_capabilities(const terse_capabilities_t *caps)
{
	printf("Detected profile: P%d\n", caps->profile);
	printf("Notifications: %s%s%s\n",
		(caps->notifications & TERSE_NOTIFICATION_SUPPORT_BELL) ? "bell " : "",
		(caps->notifications & TERSE_NOTIFICATION_SUPPORT_VISUAL) ? "visual " : "",
		(caps->notifications & TERSE_NOTIFICATION_SUPPORT_DESKTOP) ? "desktop" : "");
	fflush(stdout);
}

static void send_desktop_notification(terse_handle_t handle, const char *message)
{
	int rc = terse_notify(handle, TERSE_NOTIFICATION_KIND_DESKTOP, message);
	if (rc < 0) {
		fprintf(stderr, "Failed to send desktop notification: %s\n", strerror(-rc));
	} else {
		printf("Desktop notification requested: %s\n", message);
	}
}

static void send_bell(terse_handle_t handle)
{
	int rc = terse_notify(handle, TERSE_NOTIFICATION_KIND_BELL, NULL);
	if (rc < 0) {
		fprintf(stderr, "Bell request failed: %s\n", strerror(-rc));
	} else {
		printf("Bell sent.\n");
	}
}

static void send_visual_bell(terse_handle_t handle)
{
	int rc = terse_notify(handle, TERSE_NOTIFICATION_KIND_VISUAL, NULL);
	if (rc < 0) {
		fprintf(stderr, "Visual bell failed: %s\n", strerror(-rc));
	} else {
		printf("Visual bell requested.\n");
	}
}

int main(void)
{
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_NOTIFICATION_BELL | TERSE_CAP_ENABLE_NOTIFICATION_VISUAL | TERSE_CAP_ENABLE_NOTIFICATION_DESKTOP,
	};

	terse_handle_t handle = terse_open(TERSE_P3, &options);
	if (!handle) {
		perror("terse_open");
		return EXIT_FAILURE;
	}

	terse_capabilities_t caps = terse_get_capabilities(handle);
	print_capabilities(&caps);

	printf("Commands:\n");
	printf("  b <message>  - send desktop notification with <message>\n");
	printf("  v            - trigger visual bell\n");
	printf("  g            - ring bell (BEL)\n");
	printf("  q            - quit\n\n");
	printf("Example: b Hello from TERSE!\n\n");
	fflush(stdout);

	char line[256];
	while (fputs("> ", stdout) && fflush(stdout) == 0 && fgets(line, sizeof(line), stdin)) {
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') {
			line[len - 1] = '\0';
		}
		if (line[0] == '\0') {
			continue;
		}
		if (line[0] == 'q') {
			break;
		}
		if (line[0] == 'g') {
			send_bell(handle);
			continue;
		}
		if (line[0] == 'v') {
			send_visual_bell(handle);
			continue;
		}
		if (line[0] == 'b') {
			const char *message = line + 1;
			while (*message == ' ') {
				++message;
			}
			if (*message == '\0') {
				printf("Usage: b <message>\n");
			} else {
				send_desktop_notification(handle, message);
			}
			continue;
		}
		printf("Unknown command: %s\n", line);
	}

	terse_close(handle);
	return EXIT_SUCCESS;
}
