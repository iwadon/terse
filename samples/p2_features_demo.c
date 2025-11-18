#include "terse.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static struct termios g_original_termios;
static int g_raw_installed = 0;
static volatile sig_atomic_t g_stop = 0;

static void restore_terminal(void)
{
	if (g_raw_installed) {
		(void)tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
		g_raw_installed = 0;
	}
}

static int install_raw_terminal(void)
{
	if (tcgetattr(STDIN_FILENO, &g_original_termios) != 0) {
		return -1;
	}
	struct termios raw = g_original_termios;
	raw.c_lflag &= ~(ICANON | ECHO | IEXTEN | ISIG);
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_cflag |= CS8;
	raw.c_oflag &= ~(OPOST);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
		return -1;
	}
	g_raw_installed = 1;
	atexit(restore_terminal);
	return 0;
}

static void handle_signal(int sig)
{
	(void)sig;
	g_stop = 1;
}

static void install_signal_handlers(void)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
}

static void print_error(const char *label, terse_handle_t handle)
{
	terse_error_t err = terse_get_last_error(handle);
	fprintf(stderr, "%s failed: %d\r\n", label, err);
}

static void describe_mouse_event(const terse_event_t *ev)
{
	const char *type = "";
	switch (ev->type) {
	case TERSE_EVENT_MOUSE_DOWN:
		type = "Down";
		break;
	case TERSE_EVENT_MOUSE_UP:
		type = "Up";
		break;
	case TERSE_EVENT_MOUSE_MOVE:
		type = "Move";
		break;
	case TERSE_EVENT_MOUSE_SCROLL:
		type = "Scroll";
		break;
	default:
		type = "Unknown";
		break;
	}
	const char *button = "";
	switch (ev->data.mouse.button) {
	case TERSE_MOUSE_BUTTON_LEFT:
		button = "Left";
		break;
	case TERSE_MOUSE_BUTTON_MIDDLE:
		button = "Middle";
		break;
	case TERSE_MOUSE_BUTTON_RIGHT:
		button = "Right";
		break;
	case TERSE_MOUSE_BUTTON_SCROLL_UP:
		button = "WheelUp";
		break;
	case TERSE_MOUSE_BUTTON_SCROLL_DOWN:
		button = "WheelDown";
		break;
	default:
		button = "None";
		break;
	}
	printf("Mouse %-6s button=%s row=%d col=%d mods=0x%x\r\n",
		type,
		button,
		ev->data.mouse.row,
		ev->data.mouse.col,
		ev->data.mouse.mods);
}

static void describe_event(const terse_event_t *ev)
{
	switch (ev->type) {
	case TERSE_EVENT_CHAR:
		printf("Char: %u\r\n", ev->data.ch.scalar);
		break;
	case TERSE_EVENT_ENTER:
		printf("Enter pressed\r\n");
		break;
	case TERSE_EVENT_MOUSE_DOWN:
	case TERSE_EVENT_MOUSE_UP:
	case TERSE_EVENT_MOUSE_MOVE:
	case TERSE_EVENT_MOUSE_SCROLL:
		describe_mouse_event(ev);
		break;
	case TERSE_EVENT_PASTE_BEGIN:
		printf("Paste begin\r\n");
		break;
	case TERSE_EVENT_PASTE_END:
		printf("Paste end\r\n");
		break;
	case TERSE_EVENT_RESIZE:
		printf("Resize: %d x %d\r\n", ev->data.resize.rows, ev->data.resize.cols);
		break;
	case TERSE_EVENT_RAW_SEQUENCE:
		printf("Raw sequence (%zu bytes)\r\n", ev->data.raw.length);
		break;
	default:
		printf("Unhandled event type %d\r\n", ev->type);
		break;
	}
}

int main(void)
{
	if (install_raw_terminal() < 0) {
		perror("install_raw_terminal");
		return 1;
	}

	install_signal_handlers();

	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TEXT_STYLES | TERSE_CAP_ENABLE_SGR_BASIC | TERSE_CAP_ENABLE_MOUSE | TERSE_CAP_ENABLE_BRACKETED_PASTE | TERSE_CAP_ENABLE_TITLE | TERSE_CAP_ENABLE_HYPERLINK,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	if (!handle) {
		fprintf(stderr, "terse_open failed\r\n");
		return 1;
	}

	terse_set_title(handle, "Terse P2 demo");
	terse_set_hyperlink(handle, "https://example.com", "Example hyperlink\r\n");

	if (terse_enable_mouse(handle, TERSE_MOUSE_SGR) < 0) {
		print_error("enable_mouse", handle);
	}
	if (terse_enable_bracketed_paste(handle) < 0) {
		print_error("enable_bracketed_paste", handle);
	}

	printf("P2 demo running. Actions:\r\n");
	printf("  - Move/click the mouse inside the terminal\r\n");
	printf("  - Scroll the wheel\r\n");
	printf("  - Try bracketed paste (e.g. copy text)\r\n");
	printf("  - Press 'q' to quit\r\n\r\n");

	while (!g_stop) {
		terse_event_t event;
		int result = terse_read_event(handle, 200, &event);
		if (result == TERSE_ERR_NO_EVENT) {
			continue;
		}
		if (result < 0) {
			print_error("read_event", handle);
			break;
		}
		describe_event(&event);
		if (event.type == TERSE_EVENT_CHAR && event.data.ch.scalar == 'q') {
			break;
		}
	}

	terse_disable_bracketed_paste(handle);
	terse_disable_mouse(handle);
	terse_set_title(handle, "");
	terse_set_hyperlink(handle, "", "");

	terse_close(handle);
	printf("Demo finished.\r\n");
	return 0;
}
