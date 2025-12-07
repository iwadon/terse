#ifndef TERSE_ENABLE_TEST_MODE
#error "This sample requires TERSE_ENABLE_TEST_MODE to be defined"
#endif

#include "terse.h"
#include "terse_test.h"

#include <stdio.h>
#include <string.h>

#include "sample_compat.h"

static void print_separator(void)
{
	printf("\n========================================\n\n");
}

// Demo 1: Recording API calls
static void demo_recording(terse_handle_t handle)
{
	printf("Demo 1: Recording API Calls\n");
	printf("Recording terse API calls...\n\n");

	terse_test_start_recording(handle);

	// Make various API calls
	terse_move_to(handle, 4, 9);
	terse_write_text(handle, "Hello, World!");
	terse_clear_screen(handle, TERSE_CLEAR_ALL);
	terse_show_cursor(handle, 0);

	terse_style_t style = {0};
	style.foreground.kind = TERSE_COLOR_KIND_BASIC16;
	style.foreground.data.basic16.color = TERSE_BASIC_COLOR_RED;
	style.foreground.data.basic16.bright = 0;
	style.effects = TERSE_STYLE_BOLD;
	terse_set_style(handle, &style);

	terse_test_stop_recording(handle);

	// Retrieve and display recorded calls
	int count = 0;
	const terse_call_record_t *calls = terse_test_get_calls(handle, &count);

	printf("Recorded %d API calls:\n", count);
	for (int i = 0; i < count; i++) {
		const terse_call_record_t *call = &calls[i];
		printf("  [%d] ", i + 1);

		switch (call->type) {
		case TERSE_CALL_MOVE_TO:
			printf("move_to(row=%d, col=%d)\n",
				call->data.move_to.row, call->data.move_to.col);
			break;
		case TERSE_CALL_WRITE_TEXT:
			printf("write_text(\"%s\")\n", call->data.write_text.text);
			break;
		case TERSE_CALL_CLEAR_SCREEN:
			printf("clear_screen(mode=%d)\n", call->data.clear_screen.mode);
			break;
		case TERSE_CALL_SHOW_CURSOR:
			printf("show_cursor(visible=%d)\n", call->data.show_cursor.visible);
			break;
		case TERSE_CALL_SET_STYLE:
			printf("set_style(fg_kind=%d, effects=0x%x)\n",
				call->data.set_style.style.foreground.kind,
				call->data.set_style.style.effects);
			break;
		case TERSE_CALL_FLUSH:
			printf("flush()\n");
			break;
		default:
			printf("(type %d)\n", call->type);
			break;
		}
	}

	terse_test_clear_calls(handle);
	printf("\nRecording cleared.\n");
}

// Demo 2: Mocking capabilities
static void demo_mock_capabilities(terse_handle_t handle)
{
	printf("Demo 2: Mocking Capabilities\n");
	printf("Setting mock capabilities...\n\n");

	terse_capabilities_t mock_caps = {0};
	mock_caps.profile = TERSE_P3;
	mock_caps.colors = TERSE_COLOR_TRUECOLOR;
	mock_caps.has_truecolor = 1;
	mock_caps.mouse = TERSE_MOUSE_SGR;
	mock_caps.has_bracketed_paste = 1;
	mock_caps.has_title = 1;
	mock_caps.has_clipboard_write = 1;
	mock_caps.images = TERSE_IMAGE_KITTY;
	mock_caps.notifications = TERSE_NOTIFICATION_SUPPORT_DESKTOP;

	terse_test_mock_capabilities(handle, &mock_caps);

	terse_capabilities_t caps = terse_get_capabilities(handle);

	printf("Retrieved capabilities (mocked):\n");
	printf("  profile: P%d\n", caps.profile);
	printf("  colors: %d (0=none, 1=16, 2=256, 3=truecolor)\n", caps.colors);
	printf("  has_truecolor: %d\n", caps.has_truecolor);
	printf("  mouse: %d (3=SGR)\n", caps.mouse);
	printf("  has_bracketed_paste: %d\n", caps.has_bracketed_paste);
	printf("  has_title: %d\n", caps.has_title);
	printf("  has_clipboard_write: %d\n", caps.has_clipboard_write);
	printf("  images: %d (3=kitty)\n", caps.images);
	printf("  notifications: 0x%x\n", caps.notifications);

	printf("\nCapabilities successfully mocked.\n");
}

// Demo 3: Mocking terminal size
static void demo_mock_size(terse_handle_t handle)
{
	printf("Demo 3: Mocking Terminal Size\n");
	printf("Setting mock size to 80x24...\n\n");

	terse_test_mock_size(handle, 24, 80);

	terse_size_t size = terse_get_size(handle);

	printf("Retrieved size (mocked):\n");
	printf("  rows: %d\n", size.rows);
	printf("  cols: %d\n", size.cols);
	printf("  known: %d\n", size.known);

	if (size.rows == 24 && size.cols == 80) {
		printf("\nSize mock verified successfully.\n");
	} else {
		printf("\nWarning: Size mock did not match expected values.\n");
	}
}

// Demo 4: Mocking input events
static void demo_mock_events(terse_handle_t handle)
{
	printf("Demo 4: Mocking Input Events\n");
	printf("Setting up mock events...\n\n");

	terse_event_t events[5];

	// Event 1: Character 'a'
	events[0].type = TERSE_EVENT_CHAR;
	events[0].data.ch.scalar = 'a';
	events[0].data.ch.width = 1;
	events[0].data.ch.mods = 0;

	// Event 2: Character 'b' with Ctrl modifier
	events[1].type = TERSE_EVENT_CHAR;
	events[1].data.ch.scalar = 'b';
	events[1].data.ch.width = 1;
	events[1].data.ch.mods = TERSE_MOD_CTRL;

	// Event 3: Escape key
	events[2].type = TERSE_EVENT_CHAR;
	events[2].data.ch.scalar = 27; // ESC
	events[2].data.ch.width = 0;
	events[2].data.ch.mods = 0;

	// Event 4: Mouse click
	events[3].type = TERSE_EVENT_MOUSE_DOWN;
	events[3].data.mouse.button = TERSE_MOUSE_BUTTON_LEFT;
	events[3].data.mouse.row = 10;
	events[3].data.mouse.col = 20;
	events[3].data.mouse.mods = 0;

	// Event 5: Resize event
	events[4].type = TERSE_EVENT_RESIZE;
	events[4].data.resize.rows = 30;
	events[4].data.resize.cols = 100;

	terse_test_mock_events(handle, events, 5);

	printf("Reading mock events:\n");

	for (int i = 0; i < 5; i++) {
		terse_event_t event;
		int rc = terse_read_event(handle, 100, &event);

		if (rc < 0) {
			printf("  [%d] Error reading event: %d\n", i + 1, rc);
			continue;
		}
		if (rc == TERSE_ERR_NO_EVENT) {
			printf("  [%d] No event (timeout)\n", i + 1);
			continue;
		}

		printf("  [%d] ", i + 1);
		switch (event.type) {
		case TERSE_EVENT_CHAR:
			if (event.data.ch.scalar == 27) {
				printf("ESC key\n");
			} else if (event.data.ch.mods & TERSE_MOD_CTRL) {
				printf("Char '%c' (Ctrl+%c)\n", event.data.ch.scalar,
					event.data.ch.scalar);
			} else {
				printf("Char '%c'\n", event.data.ch.scalar);
			}
			break;
		case TERSE_EVENT_MOUSE_DOWN:
			printf("Mouse DOWN (button=%d, row=%d, col=%d)\n",
				event.data.mouse.button, event.data.mouse.row,
				event.data.mouse.col);
			break;
		case TERSE_EVENT_RESIZE:
			printf("Resize (%dx%d)\n", event.data.resize.rows,
				event.data.resize.cols);
			break;
		default:
			printf("Event type %d\n", event.type);
			break;
		}
	}

	printf("\nAll mock events successfully read.\n");
}

int main(void)
{
	printf("Terse Test Mode Demo\n");
	printf("====================\n");
	printf("This demo showcases the test mode recording and mocking features.\n");

	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0
	};

	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
	if (!handle) {
		fprintf(stderr, "terse_open failed\n");
		return 1;
	}

	print_separator();
	demo_recording(handle);

	print_separator();
	demo_mock_capabilities(handle);

	print_separator();
	demo_mock_size(handle);

	print_separator();
	demo_mock_events(handle);

	print_separator();
	printf("Resetting all mocks...\n");
	terse_test_reset_mocks(handle);
	printf("All mocks reset.\n");

	terse_close(handle);

	printf("\nDemo completed successfully.\n");
	return 0;
}
