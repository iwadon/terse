/*
 * Cursor Position Test
 * Tests terse_get_cursor_position() functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <terse.h>

int main(void)
{
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	if (!handle) {
		fprintf(stderr, "Failed to open terse\n");
		return 1;
	}

	printf("Cursor Position Test\n");
	printf("===================\n\n");

	/* Get initial cursor position */
	terse_cursor_position_t pos = terse_get_cursor_position(handle);
	if (pos.known) {
		printf("Initial cursor position: row=%d, col=%d\n", pos.row, pos.col);
	} else {
		printf("Failed to get cursor position (not supported or error)\n");
		terse_error_t err = terse_get_last_error(handle);
		printf("Error: %d\n", err);
	}

	/* Move cursor and check position */
	printf("\nMoving cursor to (5, 10)...\n");
	terse_move_to(handle, 5, 10);
	terse_flush(handle);

	pos = terse_get_cursor_position(handle);
	if (pos.known) {
		printf("Cursor position after move: row=%d, col=%d\n", pos.row, pos.col);
		if (pos.row == 5 && pos.col == 10) {
			printf("SUCCESS: Position matches expected value\n");
		} else {
			printf("WARNING: Position mismatch (expected 5, 10)\n");
		}
	} else {
		printf("Failed to get cursor position after move\n");
	}

	/* Write some text and check position */
	printf("\nWriting 'Hello' at current position...\n");
	terse_write_text(handle, "Hello");
	terse_flush(handle);

	pos = terse_get_cursor_position(handle);
	if (pos.known) {
		printf("\nCursor position after write: row=%d, col=%d\n", pos.row, pos.col);
		printf("(Expected: row=5, col=15 or nearby)\n");
	}

	/* Move back to a safe position before closing */
	terse_move_to(handle, 20, 0);
	terse_flush(handle);

	terse_close(handle);
	printf("\nTest complete.\n");
	return 0;
}
