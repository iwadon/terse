# Human68k Keyboard Input

This document describes keyboard input handling on the X68000/Human68k platform, including low-level DOS API details, known issues, and debugging techniques.

## Overview

Human68k keyboard input differs significantly from POSIX terminal input:
- Uses DOS API calls (`_dos_inkey()`, `_dos_keysns()`) instead of file descriptor reads
- Returns combined scan code + character code in a single 16-bit value
- No escape sequence parsing (no CSI sequences like POSIX terminals)
- Native Shift_JIS encoding for Japanese character input
- X68000-specific function and control keys

## Keyboard Input API Layers

Human68k provides multiple layers of keyboard input APIs with different levels of abstraction:

### DOS API Layer (`_dos_inkey()`) - **NOT Raw Input**

⚠️ **Important**: `_dos_inkey()` does NOT return raw scan codes. It returns pre-processed input after OS key mapping.

**Limitations**:
1. **Key mapping layer**: Returns characters/strings assigned to keys, not raw key events
   - Function keys (F1-F10) return their assigned strings (e.g., "help", "dir", etc.)
   - Physical keys are translated through the OS key map
2. **Modifier keys invisible**: X68000-specific modifiers are not reported
   - XF1, XF2, XF3, XF4, XF5 (extended function keys used as modifiers)
   - OPT.1, OPT.2 (option keys)
   - These act as modifiers but don't appear in `_dos_inkey()` results
3. **OS intercepts special keys**:
   - Ctrl+C: OS catches this for program interruption (cannot be intercepted by application)
   - Ctrl+S/Ctrl+Q: Flow control (may be handled by OS)

**Result format** (when it does return):
```
Bits 15-8: Scan Code (approximate, post-mapping)
Bits 7-0:  Character Code (ASCII or Shift_JIS byte)
```

For true raw keyboard input, lower-level APIs are needed (IOCS or direct hardware access).

### Examples

| Key Press | Raw Value | Scan Code | Char Code | Description |
|-----------|-----------|-----------|-----------|-------------|
| 'a'       | 0x001E61  | 0x1E      | 0x61      | Normal character 'a' |
| 'A' (Shift+a) | 0x1E41 | 0x1E   | 0x41      | Uppercase 'A' |
| F1        | 0x3B00    | 0x3B      | 0x00      | Function key F1 |
| Arrow Up  | 0x4800    | 0x48      | 0x00      | Cursor up |
| Enter     | 0x1C0D    | 0x1C      | 0x0D      | Return key |
| ESC       | 0x011B    | 0x01      | 0x1B      | Escape key |

## X68000 Scan Codes

### Function Keys

| Scan Code | Key    | Notes |
|-----------|--------|-------|
| 0x3B      | F1     | |
| 0x3C      | F2     | |
| 0x3D      | F3     | |
| 0x3E      | F4     | |
| 0x3F      | F5     | |
| 0x40      | F6     | |
| 0x41      | F7     | |
| 0x42      | F8     | |
| 0x43      | F9     | |
| 0x44      | F10    | |

### X68000-Specific Function Keys

| Scan Code | Key    | Notes |
|-----------|--------|-------|
| 0x62      | XF1    | X68000 extended function key |
| 0x63      | XF2    | X68000 extended function key |
| 0x64      | XF3    | X68000 extended function key |
| 0x65      | XF4    | X68000 extended function key |
| 0x66      | XF5    | X68000 extended function key |

### Cursor Movement Keys

| Scan Code | Key         | Notes |
|-----------|-------------|-------|
| 0x47      | HOME        | |
| 0x48      | UP          | Arrow up |
| 0x49      | ROLLUP      | Page up |
| 0x4B      | LEFT        | Arrow left |
| 0x4D      | RIGHT       | Arrow right |
| 0x4F      | END         | |
| 0x50      | DOWN        | Arrow down |
| 0x51      | ROLLDOWN    | Page down |
| 0x52      | INS         | Insert |
| 0x53      | DEL         | Delete |

### Other Special Keys

| Scan Code | Key    | Notes |
|-----------|--------|-------|
| 0x01      | ESC    | Escape |
| 0x0E      | BS     | Backspace |
| 0x0F      | TAB    | Tab |
| 0x1C      | RETURN | Enter/Return |
| 0x54      | CLR    | Clear screen (X68000-specific) |
| 0x5A      | HELP   | Help key (X68000-specific) |
| 0x5B      | HOME   | Alternative HOME (via CLR key) |

## Shift_JIS Japanese Input

Human68k uses Shift_JIS encoding for Japanese text. Multibyte characters are input as separate `_dos_inkey()` calls:

### Multibyte Sequence Example

For the Japanese character "あ" (Hiragana A, Shift_JIS: 0x82A0):

1. First call: `_dos_inkey()` returns high byte (0x82xx)
2. Second call: `_dos_inkey()` returns low byte (0xxA0)

The application must buffer the first byte and combine it with the second byte to form the complete Shift_JIS character.

### Shift_JIS Byte Ranges

- **Lead bytes**: 0x81-0x9F, 0xE0-0xEF
- **Trail bytes**: 0x40-0x7E, 0x80-0xFC
- **ASCII range**: 0x00-0x7F (single byte)

## Known Issues and Differences from POSIX

### 1. Duplicate RAW and Normal Key Events

**Symptom**: When pressing X68000-specific keys (XF1-XF5, CLR, HELP), both a RAW byte sequence and a normal key event may be generated.

**Cause**:
- `_dos_inkey()` returns mapped character strings, not raw scan codes
- Function keys return multi-byte strings (e.g., "help", "dir")
- Current implementation in `terse_human68k.c` uses `terse_platform_read_byte()` which only reads one byte at a time
- Multi-byte function key strings are split across multiple reads, causing parsing confusion

**Root issue**: DOS API is not suitable for raw keyboard input. Need lower-level API (IOCS or hardware access).

### 2. Japanese Character Display Issues

**Symptom**: Japanese text may appear garbled or incorrectly aligned when displayed through `event_logger_demo_human68k`.

**Possible Causes**:
- Cell width calculation issues for double-width characters
- Shift_JIS multibyte handling in the event parser
- Terminal emulator rendering issues

**Workaround**: (To be documented after investigation)

### 3. Modifier Keys Not Visible

**Symptom**: X68000-specific modifier keys (XF1-XF5, OPT.1, OPT.2) do not appear in keyboard input.

**Cause**: These keys function as modifiers (like Shift/Ctrl/Alt) but `_dos_inkey()` does not report modifier state. Need separate API to query modifier key state.

**Workaround**: Use IOCS keyboard matrix query or separate modifier state API (if available).

### 4. OS Intercepts Control Keys

**Symptom**: Ctrl+C terminates the program instead of being passed to the application.

**Cause**: DOS intercepts certain control key combinations for system functions:
- Ctrl+C: Program interruption
- Ctrl+S/Ctrl+Q: Terminal flow control (XOFF/XON)

**Workaround**: May require disabling OS break checking or using raw IOCS input.

### 5. No Escape Sequence Support

Unlike POSIX terminals, Human68k does not use escape sequences for special keys.

This means:
- No CSI sequences (no `\x1b[A` for arrow keys)
- No modifier key encoding in escape sequences
- All keys must be identified through scan codes or key matrix

## Debugging Tools

### keycode_dump_human68k

Low-level tool to observe `_dos_inkey()` return values:

```bash
./keycode_dump_human68k
```

This tool displays:
- Raw 16-bit value from `_dos_inkey()`
- High byte and low byte
- Interpreted key name (based on common scan codes)

**Note**: Due to `_dos_inkey()` limitations, this shows OS-mapped input, not raw scan codes:
- Function keys show their assigned strings, not raw key events
- Modifier keys (XF1-XF5, OPT.1/2) are not visible
- Ctrl+C will terminate the program

For true raw input investigation, a lower-level IOCS-based tool is needed.

### event_logger_demo_human68k

Higher-level tool that uses the terse library to show parsed events:

```bash
./event_logger_demo_human68k
```

This shows how terse interprets keypress events after parsing.

## Data Collection Guidelines

When reporting keyboard issues on Human68k, please include:

1. **Raw keycode data**: Output from `keycode_dump_human68k`
2. **Expected behavior**: What should happen
3. **Actual behavior**: What actually happens (include `event_logger_demo_human68k` output if applicable)
4. **Terminal/Emulator**: Whether on real hardware or emulator (XM6, etc.)

### Example Report Format

```
Key pressed: XF1
Raw keycode: 0x6200 (from keycode_dump_human68k)
Expected: Single FUNCTION event with number=11
Actual: RAW event (0x62 0x00) followed by FUNCTION event
Environment: XM6 Type G emulator
```

## Implementation Notes (for terse library developers)

### Current Implementation (c/src/terse_human68k.c)

**Current approach** (as of initial implementation):
- Uses DOS API (`_dos_inkey()`, `_dos_keysns()`)
- `terse_platform_read_byte()` returns only the low byte (character code)
- Loses scan code information (high byte is discarded)
- Cannot distinguish special keys properly

**Limitations discovered**:
1. `_dos_inkey()` is not raw input - it's OS-mapped input
2. Function keys return multi-character strings, not scan codes
3. Modifier keys (XF1-XF5, OPT.1/2) are invisible
4. OS intercepts Ctrl+C and other control keys
5. Single-byte read model doesn't match multi-byte function key strings

### Future Directions

To properly support Human68k keyboard input, the implementation should:

1. **Use IOCS or hardware-level keyboard access** instead of DOS API
   - Direct keyboard matrix reading
   - Raw scan code access
   - Modifier key state query

2. **Platform-specific event reading**
   - Add `terse_platform_read_event()` to platform API
   - Human68k implementation directly fills `terse_event_t` from raw input
   - Bypass byte-stream parsing model (POSIX-centric design)

3. **Handle X68000-specific features**
   - XF1-XF5 as modifier keys
   - OPT.1, OPT.2 modifier keys
   - X68000 function keys (CLR, HELP, COPY, etc.)

Implementation of lower-level keyboard access is pending investigation of available IOCS APIs.

## See Also

- `c/src/terse_human68k.c` - Platform implementation
- `samples/keycode_dump_human68k.c` - Low-level debugging tool
- `samples/event_logger_demo_human68k.c` - High-level event logger
- `docs/terse-platform-porting.md` - General platform porting guide
