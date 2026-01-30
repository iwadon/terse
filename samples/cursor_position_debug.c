/*
 * Cursor Position Debug Tool
 * Low-level test of cursor position query with detailed logging
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <x68k/dos.h>

static void print_bytes(const char *label, unsigned char *buffer, size_t len)
{
	printf("%s (%zu bytes): ", label, len);
	for (size_t i = 0; i < len; i++) {
		if (buffer[i] >= 32 && buffer[i] < 127) {
			printf("%c", buffer[i]);
		} else {
			printf("[%02X]", buffer[i]);
		}
	}
	printf("\r\n");
}

int main(void)
{
	printf("Cursor Position Query Debug\r\n");
	printf("===========================\r\n\r\n");

	/* Clear any pending input */
	printf("Clearing input buffer...\r\n");
	while (_dos_keysns() != 0) {
		_dos_inkey();
	}
	printf("Input buffer cleared.\r\n\r\n");

	/* Send cursor position request */
	printf("Sending CPR request (ESC[6n)...\r\n");
	const char request[] = "\x1b[6n";
	for (size_t i = 0; i < sizeof(request) - 1; i++) {
		_dos_putchar((unsigned char)request[i]);
	}
	printf("Request sent.\r\n\r\n");

	/* Wait for response */
	printf("Waiting for response (max 2 seconds)...\r\n");
	unsigned char buffer[32];
	size_t length = 0;
	const int timeout_ms = 2000;  /* Increased timeout for debugging */
	const int poll_interval = 50; /* 50ms intervals */
	int elapsed = 0;

	while (length < sizeof(buffer) && elapsed < timeout_ms) {
		if (_dos_keysns() != 0) {
			int ch = _dos_inkey();
			buffer[length++] = (unsigned char)(ch & 0xFF);
			printf("Read byte %zu: 0x%02X ('%c')\r\n",
			       length, buffer[length - 1],
			       (buffer[length - 1] >= 32 && buffer[length - 1] < 127) ? buffer[length - 1] : '?');

			/* Check if complete */
			if (buffer[length - 1] == 'R') {
				printf("\r\nComplete response received!\r\n");
				break;
			}
		} else {
			usleep(poll_interval * 1000);
			elapsed += poll_interval;
		}
	}

	printf("\r\nTimeout: %d ms elapsed\r\n", elapsed);
	printf("Total bytes received: %zu\r\n\r\n", length);

	if (length == 0) {
		printf("ERROR: No response received from terminal.\r\n");
		printf("Terminal may not support CPR (ESC[6n).\r\n");
		return 1;
	}

	print_bytes("Received data", buffer, length);

	/* Parse response */
	printf("\r\nParsing response...\r\n");

	if (length < 6) {
		printf("ERROR: Response too short (minimum 6 bytes: ESC[r;cR)\r\n");
		return 1;
	}

	if (buffer[0] != 0x1b) {
		printf("ERROR: Expected ESC (0x1B), got 0x%02X\r\n", buffer[0]);
		return 1;
	}

	if (buffer[1] != '[') {
		printf("ERROR: Expected '[' (0x5B), got 0x%02X\r\n", buffer[1]);
		return 1;
	}

	if (buffer[length - 1] != 'R') {
		printf("ERROR: Expected 'R' at end, got 0x%02X\r\n", buffer[length - 1]);
		return 1;
	}

	/* Parse row */
	int row = 0;
	size_t i = 2;
	printf("Parsing row starting at index %zu...\r\n", i);
	while (i < length && buffer[i] >= '0' && buffer[i] <= '9') {
		row = row * 10 + (buffer[i] - '0');
		printf("  Digit '%c' -> row=%d\r\n", buffer[i], row);
		i++;
	}

	if (i >= length || buffer[i] != ';') {
		printf("ERROR: Expected ';' separator at index %zu, got 0x%02X\r\n", i, buffer[i]);
		return 1;
	}
	printf("Found separator ';' at index %zu\r\n", i);

	/* Parse column */
	i++; /* skip ';' */
	int col = 0;
	printf("Parsing column starting at index %zu...\r\n", i);
	while (i < length && buffer[i] >= '0' && buffer[i] <= '9') {
		col = col * 10 + (buffer[i] - '0');
		printf("  Digit '%c' -> col=%d\r\n", buffer[i], col);
		i++;
	}

	if (i >= length || buffer[i] != 'R') {
		printf("ERROR: Expected 'R' terminator at index %zu, got 0x%02X\r\n", i, buffer[i]);
		return 1;
	}

	printf("\r\nSUCCESS!\r\n");
	printf("Terminal returned: row=%d, col=%d (1-based)\r\n", row, col);
	printf("Converted to 0-based: row=%d, col=%d\r\n", row - 1, col - 1);

	return 0;
}
