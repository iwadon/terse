/*
 * Keycode Dump Tool for Human68k
 * Low-level debugging tool to observe raw _dos_inkey() return values
 * Does not use terse library - shows pure DOS API keyboard input
 */

#include <stdio.h>
#include <stdlib.h>
#include <x68k/dos.h>
#include <x68k/iocs.h>

static void print_key_info(unsigned int keycode)
{
	unsigned char lo = keycode & 0xFF;		  /* Character code */
	unsigned char hi = (keycode >> 8) & 0xFF; /* Scan code */

	printf("Raw: 0x%04X  ", keycode);
	printf("Scan: 0x%02X  ", hi);
	printf("Char: 0x%02X  ", lo);

	/* Try to display character if printable */
	if (lo >= 0x20 && lo < 0x7F) {
		printf("'%c'  ", lo);
	} else if (lo >= 0x80) {
		/* Might be Shift_JIS */
		printf("[0x%02X]  ", lo);
	} else {
		/* Control character */
		printf("^%c  ", lo + 0x40);
	}

	/* Common scan code meanings for X68000 */
	printf("| ");
	switch (hi) {
	case 0x00:
		printf("(normal char)");
		break;
	case 0x01:
		printf("ESC");
		break;
	case 0x0f:
		printf("BS");
		break;
	case 0x10:
		printf("TAB");
		break;
	case 0x1d:
		printf("RETURN");
		break;
	case 0x35:
		printf("SPACE");
		break;
	case 0x36:
		printf("HOME");
		break;
	case 0x37:
		printf("DEL");
		break;
	case 0x38:
		printf("ROLL UP");
		break;
	case 0x39:
		printf("ROLL DOWN");
		break;
	case 0x3a:
		printf("UNDO");
		break;
	case 0x3b:
		printf("LEFT");
		break;
	case 0x3c:
		printf("UP");
		break;
	case 0x3d:
		printf("RIGHT");
		break;
	case 0x3e:
		printf("DOWN");
		break;
	case 0x3f:
		printf("CLR");
		break;
	case 0x4e:
		printf("ENTER"); /* テンキー */
		break;
	case 0x52:
		printf("KIGOU"); /* 記号 */
		break;
	case 0x53:
		printf("TOROKU"); /* 登録 */
		break;
	case 0x54:
		printf("HELP");
		break;
	case 0x55:
		printf("XF1");
		break;
	case 0x56:
		printf("XF2");
		break;
	case 0x57:
		printf("XF3");
		break;
	case 0x58:
		printf("XF4");
		break;
	case 0x59:
		printf("XF5");
		break;
	case 0x5a:
		printf("かな (toggle)");
		break;
	case 0x5b:
		printf("ローマ字 (toggle)");
		break;
	case 0x5c:
		printf("コード (toggle)");
		break;
	case 0x5d:
		printf("CAPS");
		break;
	case 0x5e:
		printf("INS");
		break;
	case 0x5f:
		printf("ひらがな (toggle)");
		break;
	case 0x60:
		printf("全角 (toggle)");
		break;
	case 0x61:
		printf("BREAK");
		break;
	case 0x62:
		printf("COPY");
		break;
	case 0x63:
		printf("F1");
		break;
	case 0x64:
		printf("F2");
		break;
	case 0x65:
		printf("F3");
		break;
	case 0x66:
		printf("F4");
		break;
	case 0x67:
		printf("F5");
		break;
	case 0x68:
		printf("F6");
		break;
	case 0x69:
		printf("F7");
		break;
	case 0x6a:
		printf("F8");
		break;
	case 0x6b:
		printf("F9");
		break;
	case 0x6c:
		printf("F10");
		break;
	case 0x70:
		printf("SHIFT");
		break;
	case 0x71:
		printf("CTRL");
		break;
	case 0x72:
		printf("OPT.1");
		break;
	case 0x73:
		printf("OPT.2");
		break;
	case 0x74:
		printf("NUM");
		break;
	case 0xf0:
		printf("SHIFT (release)");
		break;
	case 0xf1:
		printf("CTRL (release)");
		break;
	case 0xf2:
		printf("OPT.1 (release)");
		break;
	case 0xf3:
		printf("OPT.2 (release)");
		break;
	default:
		if (hi >= 0x80) {
			printf("(Shift_JIS lead byte?)");
		} else if (hi != 0) {
			printf("(scan 0x%02X)", hi);
		}
		break;
	}

	printf("\r\n");
}

int main(void)
{
	printf("===================================\r\n");
	printf("  X68000 Keycode Dump Tool\r\n");
	printf("===================================\r\n");
	printf("Press keys to see raw _dos_inkey() values.\r\n");
	printf("Press 'q' to quit.\r\n");
	printf("\r\n");
	printf("Format: Raw (full 16-bit) | Scan (high byte) | Char (low byte) | Info\r\n");
	printf("-----------------------------------\r\n");

	/* Clear any pending input */
	while (_iocs_b_keysns() != 0) {
		_iocs_b_keyinp();
	}

	int count = 0;
	while (1) {
		/* Poll for input */
		if (_iocs_b_keysns() == 0) {
			continue;
		}

		/* Read key */
		unsigned int keycode = _iocs_b_keyinp(); // => high 8bits: scan code, low 8bits: ascii code

		/* Display with sequence number */
		printf("#%-4d ", ++count);
		print_key_info(keycode);

		/* Exit on 'q' or 'Q' */
		unsigned char lo = keycode & 0xff;
		if (lo == 'q' || lo == 'Q') {
			printf("\r\nQuitting...\r\n");
			break;
		}
	}

	return 0;
}
