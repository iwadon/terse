/*
 * Keycode Dump Tool for Human68k
 * Low-level debugging tool to observe raw _dos_inkey() return values
 * Does not use terse library - shows pure DOS API keyboard input
 */

#include <stdio.h>
#include <stdlib.h>
#include <x68k/dos.h>
#include <x68k/iocs.h>

static int count = 0;

static void print_key_info(unsigned int keysns, unsigned int mod, unsigned char *buf, int len)
{
	++count;
	printf("%4u: %05X %04X", count, keysns, mod);

	for (int i = 0; i < len; ++i) {
		printf(" %02X", (unsigned char)buf[i]);
	}
	for (int i = len; i < 2; ++i) {
		printf(" ..");
	}
	if (len == 0) {
		printf(" '^@' ");
	} else if (len == 1) {
		if (buf[0] < 0x20 || buf[0] == 0x7f) {
			printf(" '^%c' ", buf[0] ^ 0x40);
		} else if (buf[0] >= 0xa1 && buf[0] <= 0xdf) {
			printf(" '%c'  ", buf[0]);
		} else {
			printf(" '%s'  ", (char *)buf);
		}
	} else {
		printf(" '%s' ", (char *)buf);
	}

	switch ((keysns >> 8) & 0x7f) {
	case 0x01:
		printf("| ESC"); /* Escape */
		break;
	case 0x0f:
		printf("| BS"); /* Backspace */
		break;
	case 0x10:
		printf("| TAB"); /* Tab */
		break;
	case 0x1d:
		printf("| CR"); /* Enter (full) */
		break;
	case 0x35:
		printf("| SPACE"); /* Space */
		break;
	case 0x36:
		printf("| HOME"); /* Home */
		break;
	case 0x37:
		printf("| DEL"); /* Delete */
		break;
	case 0x38:
		printf("| R_UP"); /* Roll Up */
		break;
	case 0x39:
		printf("| R_DOWN"); /* Roll Down */
		break;
	case 0x3a:
		printf("| UNDO"); /* Undo */
		break;
	case 0x3b:
		printf("| ↑"); /* Up Arrow */
		break;
	case 0x3c:
		printf("| →"); /* Right Arrow */
		break;
	case 0x3d:
		printf("| ←"); /* Left Arrow */
		break;
	case 0x3e:
		printf("| ↓"); /* Down Arrow */
		break;
	case 0x3f:
		printf("| CLR"); /* Clear */
		break;
	case 0x4e:
		printf("| ENTER"); /* Enter (numeric) */
		break;
	case 0x52:
		printf("| 記号"); /* Symbol key */
		break;
	case 0x53:
		printf("| 登録"); /* Register key */
		break;
	case 0x54:
		printf("| HELP"); /* Help key */
		break;
	case 0x55:
		printf("| XF1"); /* Extra Function 1 */
		break;
	case 0x56:
		printf("| XF2"); /* Extra Function 2 */
		break;
	case 0x57:
		printf("| XF3"); /* Extra Function 3 */
		break;
	case 0x58:
		printf("| XF4"); /* Extra Function 4 */
		break;
	case 0x59:
		printf("| XF5"); /* Extra Function 5 */
		break;
	case 0x5a:
		printf("| かな"); /* Kana key */
		break;
	case 0x5b:
		printf("| ローマ字"); /* Romaji key */
		break;
	case 0x5c:
		printf("| コード入力"); /* Code Input key */
		break;
	case 0x5d:
		printf("| CAPS"); /* Caps Lock */
		break;
	case 0x5e:
		printf("| INS"); /* Insert */
		break;
	case 0x5f:
		printf("| ひらがな"); /* Hiragana key */
		break;
	case 0x60:
		printf("| 全角"); /* Zenkaku key */
		break;
	case 0x61:
		printf("| BREAK"); /* Break */
		break;
	case 0x62:
		printf("| COPY"); /* Copy */
		break;
	case 0x63:
		printf("| F1"); /* Function 1 */
		break;
	case 0x64:
		printf("| F2"); /* Function 2 */
		break;
	case 0x65:
		printf("| F3"); /* Function 3 */
		break;
	case 0x66:
		printf("| F4"); /* Function 4 */
		break;
	case 0x67:
		printf("| F5"); /* Function 5 */
		break;
	case 0x68:
		printf("| F6"); /* Function 6 */
		break;
	case 0x69:
		printf("| F7"); /* Function 7 */
		break;
	case 0x6a:
		printf("| F8"); /* Function 8 */
		break;
	case 0x6b:
		printf("| F9"); /* Function 9 */
		break;
	case 0x6c:
		printf("| F10"); /* Function 10 */
		break;
	case 0x70:
		printf("| SHIFT"); /* Shift */
		break;
	case 0x71:
		printf("| CTRL"); /* Control */
		break;
	case 0x72:
		printf("| OPT.1"); /* Option 1 (Alt) */
		break;
	case 0x73:
		printf("| OPT.2"); /* Option 2 (AltGr) */
		break;
	case 0x74:
		printf("| NUM"); /* Num Lock */
		break;
	}

	if (mod & 0x01) {
		printf(" [SHIFT]");
	}
	if (mod & 0x02) {
		printf(" [CTRL]");
	}
	if (mod & 0x04) {
		printf(" [OPT.1]");
	}
	if (mod & 0x08) {
		printf(" [OPT.2]");
	}

	printf("\n");
}

static int is_shift_jis_lead_byte(unsigned char ch)
{
	return (ch >= 0x81 && ch <= 0x9f) || (ch >= 0xe0 && ch <= 0xfc);
}

static int is_modifier_key(unsigned int keysns)
{
	unsigned int scancode = (keysns >> 8) & 0x7f;
	return scancode == 0x70 /* SHIFT */
		|| scancode == 0x71 /* CTRL */
		|| scancode == 0x72 /* OPT.1 (Alt) */
		|| scancode == 0x73 /* OPT.2 (AltGr) */
		|| scancode == 0x5a /* かな */
		|| scancode == 0x5b /* ローマ字 */
		|| scancode == 0x5c /* コード入力 */
		|| scancode == 0x5d /* CAPS */
		|| scancode == 0x5e /* INS */
		|| scancode == 0x5f /* ひらがな */
		|| scancode == 0x60 /* 全角 */
		;
}

int main(void)
{
	while (1) {
		/* キーの入力状態を調べる */
		int keysns = _iocs_b_keysns();
		if (keysns == 0) {
			/* 入力なし */
			continue;
		}

		int len = 0;

		if ((keysns & 0x00ff) == 0) {
			_iocs_b_keyinp(); /* ダミー読み取り */
			if (!is_modifier_key(keysns)) {
				unsigned int mod = _iocs_b_sftsns();
				print_key_info(keysns, mod, NULL, len);
			}
			continue;
		}

		unsigned char buf[3] = { 0 };
		buf[0] = _dos_inkey();
		len = 1;
		if (is_shift_jis_lead_byte((unsigned char)buf[0])) {
			/* 先頭バイトなら次のバイトも読む */
			unsigned int ch2 = _dos_inkey();
			buf[1] = ch2;
			len = 2;
		}
		unsigned int mod = _iocs_b_sftsns();

		print_key_info(keysns, mod, buf, len);

		if (buf[0] == 'q' || buf[0] == 'Q') {
			printf("Quitting...\r\n");
			break;
		}
	}

	return 0;
}
