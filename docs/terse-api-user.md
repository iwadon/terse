# Terse API User Guide

## Table of Contents

1. [Introduction](#introduction)
2. [Quick Start](#quick-start)
3. [Lifecycle Management](#lifecycle-management)
4. [Error Handling](#error-handling)
5. [Capability Detection](#capability-detection)
6. [P0: Basic Output](#p0-basic-output)
7. [P0: Input Events](#p0-input-events)
8. [P0: State Management](#p0-state-management)
9. [P1: Colors and Styles](#p1-colors-and-styles)
10. [P2: Advanced Input/Output](#p2-advanced-inputoutput)
11. [P3: Extended Features](#p3-extended-features)
12. [Test Mode and Mocking](#test-mode-and-mocking)
13. [Best Practices](#best-practices)

---

## Introduction

Terse is a C library for building portable terminal UI applications. It provides a profile-based system (P0-P3) that automatically detects terminal capabilities and gracefully degrades when features are unavailable.

### Profile Levels

- **P0**: Basic output (cursor movement, screen/line clearing, text output, size detection)
- **P1**: Colors and text decoration (SGR styles, 16/256/TrueColor)
- **P2**: Advanced input/output (mouse, bracketed paste, title, hyperlinks)
- **P3**: Extended features (clipboard, images, cursor shapes, notifications)

### Core Concepts

- **Handle**: Opaque `terse_handle_t` represents a terminal session
- **Capabilities**: Runtime detection with graceful degradation
- **State management**: Save/restore cursor position, visibility, and styles
- **Error handling**: Custom `terse_error_t` enum with categorized error codes

---

## Quick Start

### Minimal Example

```c
#include "terse.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    // Configure options
    terse_options_t options = {
        .input_fd = STDIN_FILENO,
        .output_fd = STDOUT_FILENO,
        .codec_name = "UTF-8",
        .disabled_caps = 0,
        .enabled_caps = 0
    };

    // Open session with automatic profile detection
    terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
    if (!handle) {
        fprintf(stderr, "Failed to open terse session\n");
        return 1;
    }

    // Clear screen and write text
    terse_clear_screen(handle, TERSE_CLEAR_ALL);
    terse_move_to(handle, 5, 10);
    terse_write_text(handle, "Hello, Terminal!");
    terse_flush(handle);

    // Cleanup
    terse_close(handle);
    return 0;
}
```

### Building

```bash
cc -I./c/include -L./build/c -lterse example.c -o example
./example
```

---

## Lifecycle Management

### Opening a Session

```c
terse_handle_t terse_open(terse_profile_t requested_profile,
                          const terse_options_t *options);
```

**Parameters:**
- `requested_profile`: Requested profile level or `TERSE_PROFILE_AUTO` for automatic detection
- `options`: Configuration options (NULL uses defaults)

**Returns:** Handle on success, NULL on failure

**Options Structure:**
```c
typedef struct terse_options {
    int input_fd;              // Input file descriptor (STDIN_FILENO)
    int output_fd;             // Output file descriptor (STDOUT_FILENO)
    const char *codec_name;    // Character encoding ("UTF-8" or "Shift_JIS")
    unsigned int disabled_caps; // Capabilities to disable
    unsigned int enabled_caps;  // Capabilities to enable
    int east_asian_ambiguous_as_wide; // Treat ambiguous-width chars as wide
} terse_options_t;
```

**Example with Options:**
```c
terse_options_t options = {
    .input_fd = STDIN_FILENO,
    .output_fd = STDOUT_FILENO,
    .codec_name = "UTF-8",
    .disabled_caps = TERSE_CAP_DISABLE_MOUSE,  // Disable mouse
    .enabled_caps = TERSE_CAP_ENABLE_TRUECOLOR, // Force TrueColor
    .east_asian_ambiguous_as_wide = 0
};

terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
```

**Important: Terminal Mode Management**

`terse_open()` does **not** automatically put the terminal into raw mode. Terse is designed to work with your application's terminal configuration rather than imposing its own settings.

- For output-only operations (cursor movement, colors, clearing screen), raw mode is not required
- For input operations (`terse_read_event()`), you must configure the terminal mode yourself using `tcsetattr()` or similar
- This design allows your application to use terminal settings appropriate to its needs (e.g., custom VMIN/VTIME, signal handling)

See the "P0: Input Events" section for details on terminal mode requirements for input handling.

### Closing a Session

```c
void terse_close(terse_handle_t handle);
```

Automatically restores terminal state:
- Re-enables cursor visibility
- Resets SGR styles
- Disables mouse tracking
- Disables bracketed paste

**Example:**
```c
terse_close(handle);  // Always call on exit
```

### Validating Options

```c
int terse_validate_options(const terse_options_t *options);
```

Validates options before opening. Returns `TERSE_OK` on success, or a `terse_error_t` code on error.

---

## Error Handling

### Return Values

Most functions return a `terse_error_t` value:
- `TERSE_OK` (0) on success
- Non-zero `terse_error_t` error code on failure

### Error Codes

```c
typedef enum terse_error {
    TERSE_OK = 0,

    /* Argument/Configuration Errors (1-99) */
    TERSE_ERR_INVALID_ARGUMENT = 1,
    TERSE_ERR_UNSUPPORTED = 2,
    TERSE_ERR_OVERFLOW = 3,

    /* State Errors (100-199) */
    TERSE_ERR_INVALID_HANDLE = 100,
    TERSE_ERR_NOT_IMPLEMENTED = 101,
    TERSE_ERR_STACK_OVERFLOW = 102,
    TERSE_ERR_STACK_UNDERFLOW = 103,

    /* I/O Transport Errors (200-299) */
    TERSE_ERR_IO = 200,
    TERSE_ERR_WOULD_BLOCK = 201,
    TERSE_ERR_NOT_TTY = 202,

    /* Protocol Errors (300-399) */
    TERSE_ERR_PROTOCOL = 300,

    /* Resource Errors (400-499) */
    TERSE_ERR_OUT_OF_MEMORY = 400,

    /* Encoding Errors (500-599) */
    TERSE_ERR_INVALID_ENCODING = 500,
    TERSE_ERR_BUFFER_TOO_SMALL = 501,
} terse_error_t;
```

Error codes are organized into ranges by category:
- **1-99**: Argument/configuration errors (invalid parameters, unsupported operations)
- **100-199**: State errors (invalid handles, stack over/underflow)
- **200-299**: I/O transport errors (I/O failures, blocking operations, TTY issues)
- **300-399**: Protocol errors (terminal protocol violations)
- **400-499**: Resource errors (memory allocation failures)
- **500-599**: Encoding errors (charset conversion issues, buffer size problems)

### Getting Last Error

```c
terse_error_t terse_get_last_error(terse_handle_t handle);
```

Returns the last error code that occurred on this handle, or `TERSE_OK` if no error.

**Example:**
```c
terse_error_t err = terse_move_to(handle, 10, 5);
if (err != TERSE_OK) {
    fprintf(stderr, "move_to failed: error code %d\n", err);

    // Can also retrieve the last error later
    terse_error_t last_err = terse_get_last_error(handle);
    fprintf(stderr, "last error: %d\n", last_err);
}
```

---

## Capability Detection

### Querying Capabilities

```c
terse_capabilities_t terse_get_capabilities(terse_handle_t handle);
```

Returns the current capabilities structure.

**Capabilities Structure:**
```c
typedef struct terse_capabilities {
    terse_profile_t profile;
    int has_basic_output;
    int has_cursor_visibility;
    int has_move_absolute;
    int has_move_relative;
    int has_clear_line;
    int has_clear_screen;
    int has_size;
    int has_sgr_basic;
    int has_sgr_extended;
    int has_truecolor;
    int has_text_styles;
    terse_mouse_mode_t mouse;
    int has_bracketed_paste;
    int has_title;
    int has_hyperlinks;
    int has_cursor_shape;
    terse_color_support_t colors;
    unsigned int effects;
    int has_clipboard_write;
    terse_image_support_t images;
    unsigned int notifications;
    unsigned int keyboard_features;
} terse_capabilities_t;
```

**Example:**
```c
terse_capabilities_t caps = terse_get_capabilities(handle);

if (caps.colors >= TERSE_COLOR_TRUECOLOR) {
    // Use 24-bit RGB colors
} else if (caps.colors >= TERSE_COLOR_PALETTE256) {
    // Use 256-color palette
} else if (caps.colors >= TERSE_COLOR_BASIC16) {
    // Use basic 16 colors
}

if (caps.mouse >= TERSE_MOUSE_SGR) {
    terse_enable_mouse(handle, TERSE_MOUSE_SGR);
}
```

### Runtime Capability Override

```c
int terse_capabilities_enable(terse_handle_t handle, unsigned int enable_mask);
int terse_capabilities_disable(terse_handle_t handle, unsigned int disable_mask);
int terse_capabilities_reset_overrides(terse_handle_t handle);
```

**Enable Flags:**
- `TERSE_CAP_ENABLE_SGR_BASIC`
- `TERSE_CAP_ENABLE_TEXT_STYLES`
- `TERSE_CAP_ENABLE_SGR_EXTENDED`
- `TERSE_CAP_ENABLE_TRUECOLOR`
- `TERSE_CAP_ENABLE_MOUSE`
- `TERSE_CAP_ENABLE_BRACKETED_PASTE`
- `TERSE_CAP_ENABLE_TITLE`
- `TERSE_CAP_ENABLE_HYPERLINK`
- `TERSE_CAP_ENABLE_CURSOR_SHAPE`
- `TERSE_CAP_ENABLE_CLIPBOARD_WRITE`
- `TERSE_CAP_ENABLE_IMAGE_INLINE`
- `TERSE_CAP_ENABLE_NOTIFICATION_*`

**Disable Flags:**
- `TERSE_CAP_DISABLE_*` (corresponding disable flags)

**Example:**
```c
// Force-enable TrueColor
terse_capabilities_enable(handle, TERSE_CAP_ENABLE_TRUECOLOR);

// Disable mouse support
terse_capabilities_disable(handle, TERSE_CAP_DISABLE_MOUSE);

// Restore automatic detection
terse_capabilities_reset_overrides(handle);
```

---

## P0: Basic Output

### Screen Clearing

```c
int terse_clear_screen(terse_handle_t handle, terse_clear_mode_t mode);
int terse_clear_line(terse_handle_t handle, terse_clear_mode_t mode);
```

**Clear Modes:**
- `TERSE_CLEAR_AFTER`: Clear from cursor to end
- `TERSE_CLEAR_BEFORE`: Clear from start to cursor
- `TERSE_CLEAR_ALL`: Clear entire screen/line

**Example:**
```c
terse_clear_screen(handle, TERSE_CLEAR_ALL);  // Clear entire screen
terse_clear_line(handle, TERSE_CLEAR_AFTER);  // Clear to end of line
```

### Cursor Movement

```c
int terse_move_to(terse_handle_t handle, int row, int col);
int terse_move_by(terse_handle_t handle, int drow, int dcol);
```

**Coordinates:** 0-based (row=0, col=0 is top-left)

**IMPORTANT:** Terse uses 0-based coordinates, like most programming languages.
- Top-left corner: `(0, 0)`
- First column: `col=0`
- First row: `row=0`
- Last row: `size.rows - 1`
- Last column: `size.cols - 1`

**Note:** Terminal escape sequences internally use 1-based coordinates, but the library handles this conversion automatically.

**Example:**
```c
terse_move_to(handle, 9, 19);   // Move to row 9, column 19 (10th row, 20th column)
terse_move_by(handle, 2, -5);   // Move down 2, left 5

// Move to top-left corner
terse_move_to(handle, 0, 0);

// Move to beginning of row 4 (5th row)
terse_move_to(handle, 4, 0);
```

### Cursor Visibility

```c
int terse_show_cursor(terse_handle_t handle, int visible);
```

**Example:**
```c
terse_show_cursor(handle, 0);  // Hide cursor
// ... draw UI ...
terse_show_cursor(handle, 1);  // Show cursor
```

### Text Output

```c
int terse_write_text(terse_handle_t handle, const char *graphemes);
int terse_flush(terse_handle_t handle);
```

**Notes:**
- `graphemes`: UTF-8 or Shift_JIS string (depending on codec)
- `terse_flush()`: Currently a no-op (output is unbuffered)

**Example:**
```c
terse_write_text(handle, "Hello, 世界!");
terse_flush(handle);
```

### Terminal Size

```c
terse_size_t terse_get_size(terse_handle_t handle);
```

**Returns:**
```c
typedef struct terse_size {
    int rows;
    int cols;
    int known;  // 0 if size is unknown
} terse_size_t;
```

**Example:**
```c
terse_size_t size = terse_get_size(handle);
if (size.known) {
    printf("Terminal: %d rows x %d cols\n", size.rows, size.cols);

    // Remember: coordinates are 0-based
    // Bottom-right corner is at (size.rows-1, size.cols-1)
    terse_move_to(handle, size.rows - 1, size.cols - 1);  // Move to bottom-right
}
```

### Cursor Position

```c
terse_cursor_position_t terse_get_cursor_position(terse_handle_t handle);
```

**Returns:**
```c
typedef struct terse_cursor_position {
    int row;
    int col;
    int known;  // 0 if position is unknown
} terse_cursor_position_t;
```

**Example:**
```c
terse_cursor_position_t pos = terse_get_cursor_position(handle);
if (pos.known) {
    printf("Cursor at: %d, %d\n", pos.row, pos.col);
}
```

---

## P0: Input Events

### Reading Events

```c
int terse_read_event(terse_handle_t handle, int timeout_ms,
                     terse_event_t *out_event);
```

**Parameters:**
- `timeout_ms`: Timeout in milliseconds (-1 for blocking)
- `out_event`: Output event structure

**Returns:**
- `TERSE_OK` (0): Event received successfully
- `TERSE_ERR_NO_EVENT` (1): Timeout, no event available
- Other `terse_error_t` values: Error occurred

**Terminal Mode Requirements:**

`terse_read_event()` requires the terminal to be in an appropriate mode for non-canonical input. Terse does **not** configure the terminal mode automatically - this is your application's responsibility.

Typical requirements:
- Non-canonical mode (ICANON disabled)
- No echo (ECHO disabled)
- VMIN and VTIME configured for your desired blocking behavior

**Example: Setting Raw Mode**
```c
#include <termios.h>

int setup_raw_mode(int fd) {
    struct termios raw;
    if (tcgetattr(fd, &raw) < 0) {
        return -1;
    }

    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    return tcsetattr(fd, TCSAFLUSH, &raw);
}

// Before using terse_read_event():
setup_raw_mode(STDIN_FILENO);
```

**Note:** See `samples/event_logger_demo.c` for a complete example of terminal mode management.

**Event Types:**
```c
typedef enum terse_event_type {
    TERSE_EVENT_CHAR,
    TERSE_EVENT_ENTER,
    TERSE_EVENT_BACKSPACE,
    TERSE_EVENT_TAB,
    TERSE_EVENT_ARROW_UP,
    TERSE_EVENT_ARROW_DOWN,
    TERSE_EVENT_ARROW_LEFT,
    TERSE_EVENT_ARROW_RIGHT,
    TERSE_EVENT_HOME,
    TERSE_EVENT_END,
    TERSE_EVENT_PAGE_UP,
    TERSE_EVENT_PAGE_DOWN,
    TERSE_EVENT_INSERT,
    TERSE_EVENT_DELETE,
    TERSE_EVENT_FUNCTION,
    TERSE_EVENT_MOUSE_DOWN,
    TERSE_EVENT_MOUSE_UP,
    TERSE_EVENT_MOUSE_MOVE,
    TERSE_EVENT_MOUSE_SCROLL,
    TERSE_EVENT_PASTE_BEGIN,
    TERSE_EVENT_PASTE_END,
    TERSE_EVENT_RESIZE,
    TERSE_EVENT_RAW_SEQUENCE
} terse_event_type_t;
```

### Event Structure

```c
typedef struct terse_event {
    terse_event_type_t type;
    union {
        struct {
            unsigned int scalar;  // Unicode codepoint
            int width;            // Display width (0, 1, or 2)
            int mods;             // Modifier keys
        } ch;
        struct {
            int mods;
        } key;
        struct {
            int mods;
            int number;           // Function key number (F1-F12)
        } function;
        struct {
            int rows;
            int cols;
        } resize;
        struct {
            terse_mouse_button_t button;
            int mods;
            int row;
            int col;
        } mouse;
        struct {
            size_t length;
            unsigned char bytes[TERSE_EVENT_RAW_MAX];
        } raw;
    } data;
} terse_event_t;
```

**Modifier Flags:**
```c
enum {
    TERSE_MOD_SHIFT = (1 << 0),
    TERSE_MOD_CTRL = (1 << 1),
    TERSE_MOD_ALT = (1 << 2),
    TERSE_MOD_META = (1 << 3),
};
```

### Event Loop Example

```c
while (1) {
    terse_event_t event;
    int result = terse_read_event(handle, 100, &event);

    if (result == TERSE_ERR_NO_EVENT) {
        continue;  // Timeout
    }
    if (result < 0) {
        break;  // Error
    }

    switch (event.type) {
        case TERSE_EVENT_CHAR:
            if (event.data.ch.scalar == 'q') {
                return;  // Quit
            }
            printf("Char: %u (width=%d, mods=0x%x)\n",
                   event.data.ch.scalar,
                   event.data.ch.width,
                   event.data.ch.mods);
            break;

        case TERSE_EVENT_ARROW_UP:
            printf("Arrow Up (mods=0x%x)\n", event.data.key.mods);
            break;

        case TERSE_EVENT_RESIZE:
            printf("Resize: %dx%d\n",
                   event.data.resize.rows,
                   event.data.resize.cols);
            break;

        case TERSE_EVENT_RAW_SEQUENCE:
            printf("Raw sequence: %zu bytes\n", event.data.raw.length);
            break;
    }
}
```

### Keyboard Feature Enhancement

```c
int terse_keyboard_enable(terse_handle_t handle, unsigned int feature_mask);
int terse_keyboard_disable(terse_handle_t handle, unsigned int feature_mask);
unsigned int terse_keyboard_get_enabled(terse_handle_t handle);
unsigned int terse_keyboard_get_supported(terse_handle_t handle);
```

**Features:**
```c
typedef enum terse_keyboard_feature {
    TERSE_KEYBOARD_FEATURE_NONE = 0,
    TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS = 1 << 0,  // xterm modifyOtherKeys
    TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL = 1 << 1,     // kitty keyboard protocol
} terse_keyboard_feature_t;
```

**Example:**
```c
// Check support
unsigned int supported = terse_keyboard_get_supported(handle);
if (supported & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) {
    // Enable enhanced key reporting
    terse_keyboard_enable(handle, TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS);
}
```

---

## P0: State Management

### Capture and Restore

```c
int terse_capture_state(terse_handle_t handle, terse_state_t *out_state);
int terse_restore_state(terse_handle_t handle, const terse_state_t *state);
```

**State Structure:**
```c
typedef struct terse_state {
    int cursor_known;
    int cursor_visible;
    int cursor_row;
    int cursor_col;
    int style_known;
    terse_style_t style;
} terse_state_t;
```

**Example:**
```c
terse_state_t saved_state;

// Save current state
terse_capture_state(handle, &saved_state);

// Modify terminal
terse_move_to(handle, 1, 1);
terse_show_cursor(handle, 0);
// ... draw UI ...

// Restore original state
terse_restore_state(handle, &saved_state);
```

### Push and Pop State Stack

```c
int terse_push_state(terse_handle_t handle);
int terse_pop_state(terse_handle_t handle);
```

**Stack Depth:** Maximum 8 levels

**Example:**
```c
// Save state on stack
terse_push_state(handle);

// Temporarily modify
terse_move_to(handle, 5, 10);
terse_write_text(handle, "Temporary message");

// Restore from stack
terse_pop_state(handle);
```

### State Override

```c
int terse_state_override(terse_handle_t handle, const terse_state_t *state);
int terse_state_clear(terse_handle_t handle);
```

**Example:**
```c
terse_state_t custom_state = {
    .cursor_known = 1,
    .cursor_visible = 1,
    .cursor_row = 10,
    .cursor_col = 5,
    .style_known = 0
};

terse_state_override(handle, &custom_state);
// ... operations ...
terse_state_clear(handle);  // Clear override
```

---

## P1: Colors and Styles

### Color Creation

```c
terse_color_t terse_color_default(void);
terse_color_t terse_color_basic(terse_basic_color_t color, int bright);
terse_color_t terse_color_palette(unsigned char index);
terse_color_t terse_color_truecolor(unsigned char r, unsigned char g, unsigned char b);
```

**Basic Colors:**
```c
typedef enum terse_basic_color {
    TERSE_BASIC_COLOR_BLACK = 0,
    TERSE_BASIC_COLOR_RED,
    TERSE_BASIC_COLOR_GREEN,
    TERSE_BASIC_COLOR_YELLOW,
    TERSE_BASIC_COLOR_BLUE,
    TERSE_BASIC_COLOR_MAGENTA,
    TERSE_BASIC_COLOR_CYAN,
    TERSE_BASIC_COLOR_WHITE
} terse_basic_color_t;
```

**Examples:**
```c
// Terminal default color
terse_color_t default_fg = terse_color_default();

// Basic 16 colors
terse_color_t red = terse_color_basic(TERSE_BASIC_COLOR_RED, 0);
terse_color_t bright_yellow = terse_color_basic(TERSE_BASIC_COLOR_YELLOW, 1);

// 256-color palette
terse_color_t palette_color = terse_color_palette(196);  // Bright red

// TrueColor (24-bit RGB)
terse_color_t orange = terse_color_truecolor(255, 165, 0);
```

### Style Management

```c
terse_style_t terse_style_default(void);
int terse_set_style(terse_handle_t handle, const terse_style_t *style);
int terse_reset_style(terse_handle_t handle, terse_reset_scope_t scope);
```

**Style Structure:**
```c
typedef struct terse_style {
    terse_color_t foreground;
    terse_color_t background;
    unsigned int effects;
} terse_style_t;
```

**Text Effects:**
```c
enum {
    TERSE_STYLE_BOLD = 1 << 0,
    TERSE_STYLE_FAINT = 1 << 1,
    TERSE_STYLE_ITALIC = 1 << 2,
    TERSE_STYLE_UNDERLINE = 1 << 3,
    TERSE_STYLE_INVERSE = 1 << 4,
    TERSE_STYLE_BLINK = 1 << 5,
    TERSE_STYLE_STRIKE = 1 << 6
};
```

**Reset Scopes:**
```c
typedef enum terse_reset_scope {
    TERSE_RESET_ALL = 0,         // Reset colors and effects
    TERSE_RESET_COLOR_ONLY,      // Reset colors only
    TERSE_RESET_EFFECTS_ONLY     // Reset effects only
} terse_reset_scope_t;
```

### Complete Style Example

```c
// Create style with TrueColor and effects
terse_style_t style = terse_style_default();
style.foreground = terse_color_truecolor(255, 100, 50);
style.background = terse_color_truecolor(20, 20, 40);
style.effects = TERSE_STYLE_BOLD | TERSE_STYLE_ITALIC;

// Apply style
terse_set_style(handle, &style);
terse_write_text(handle, "Styled text");

// Reset to default
terse_reset_style(handle, TERSE_RESET_ALL);
```

### Color Degradation

The library automatically degrades colors based on terminal capabilities:

```c
terse_capabilities_t caps = terse_get_capabilities(handle);

terse_color_t color = terse_color_truecolor(255, 128, 0);  // Orange

// Automatically degrades:
// - TrueColor terminal: RGB(255, 128, 0)
// - 256-color terminal: Closest palette match
// - 16-color terminal: Basic yellow/bright yellow
// - No color: Ignored
```

### Color Palette Example

```c
// Rainbow gradient using 256-color palette
for (int i = 0; i < 256; i++) {
    terse_style_t style = terse_style_default();
    style.background = terse_color_palette(i);
    terse_set_style(handle, &style);
    terse_write_text(handle, " ");
}
terse_reset_style(handle, TERSE_RESET_ALL);
```

---

## P2: Advanced Input/Output

### Mouse Tracking

```c
int terse_enable_mouse(terse_handle_t handle, terse_mouse_mode_t mode);
int terse_disable_mouse(terse_handle_t handle);
```

**Mouse Modes:**
```c
typedef enum terse_mouse_mode {
    TERSE_MOUSE_NONE = 0,
    TERSE_MOUSE_X10,      // Button press only
    TERSE_MOUSE_VT200,    // Press and release
    TERSE_MOUSE_SGR       // Press, release, move, scroll (recommended)
} terse_mouse_mode_t;
```

**Mouse Buttons:**
```c
typedef enum terse_mouse_button {
    TERSE_MOUSE_BUTTON_NONE = 0,
    TERSE_MOUSE_BUTTON_LEFT,
    TERSE_MOUSE_BUTTON_MIDDLE,
    TERSE_MOUSE_BUTTON_RIGHT,
    TERSE_MOUSE_BUTTON_SCROLL_UP,
    TERSE_MOUSE_BUTTON_SCROLL_DOWN
} terse_mouse_button_t;
```

**Example:**
```c
// Enable SGR mouse tracking
terse_enable_mouse(handle, TERSE_MOUSE_SGR);

while (1) {
    terse_event_t event;
    if (terse_read_event(handle, -1, &event) != TERSE_OK) {
        break;
    }

    switch (event.type) {
        case TERSE_EVENT_MOUSE_DOWN:
            printf("Mouse down: button=%d, pos=(%d,%d), mods=0x%x\n",
                   event.data.mouse.button,
                   event.data.mouse.row,
                   event.data.mouse.col,
                   event.data.mouse.mods);
            break;

        case TERSE_EVENT_MOUSE_MOVE:
            printf("Mouse move: pos=(%d,%d)\n",
                   event.data.mouse.row,
                   event.data.mouse.col);
            break;

        case TERSE_EVENT_MOUSE_SCROLL:
            if (event.data.mouse.button == TERSE_MOUSE_BUTTON_SCROLL_UP) {
                printf("Scroll up\n");
            } else {
                printf("Scroll down\n");
            }
            break;
    }
}

// Disable mouse tracking
terse_disable_mouse(handle);
```

### Bracketed Paste

```c
int terse_enable_bracketed_paste(terse_handle_t handle);
int terse_disable_bracketed_paste(terse_handle_t handle);
```

Bracketed paste mode allows detecting pasted text vs. typed text.

**Example:**
```c
terse_enable_bracketed_paste(handle);

int in_paste = 0;
while (1) {
    terse_event_t event;
    if (terse_read_event(handle, -1, &event) != TERSE_OK) {
        break;
    }

    switch (event.type) {
        case TERSE_EVENT_PASTE_BEGIN:
            printf("Paste started\n");
            in_paste = 1;
            break;

        case TERSE_EVENT_PASTE_END:
            printf("Paste ended\n");
            in_paste = 0;
            break;

        case TERSE_EVENT_CHAR:
            if (in_paste) {
                printf("Pasted char: %u\n", event.data.ch.scalar);
            } else {
                printf("Typed char: %u\n", event.data.ch.scalar);
            }
            break;
    }
}

terse_disable_bracketed_paste(handle);
```

### Window Title

```c
int terse_set_title(terse_handle_t handle, const char *title);
```

**Example:**
```c
terse_set_title(handle, "My Awesome TUI App v1.0");

// Clear title
terse_set_title(handle, "");
```

### Hyperlinks

```c
int terse_set_hyperlink(terse_handle_t handle, const char *url, const char *label);
```

Creates clickable hyperlinks in supported terminals (iTerm2, modern terminals).

**Example:**
```c
// Create hyperlink
terse_set_hyperlink(handle, "https://github.com/", "GitHub");

// End hyperlink
terse_set_hyperlink(handle, "", "");

// Full example
terse_write_text(handle, "Visit ");
terse_set_hyperlink(handle, "https://example.com", "example.com");
terse_set_hyperlink(handle, "", "");
terse_write_text(handle, " for more info.");
```

---

## P3: Extended Features

### Clipboard

```c
int terse_set_clipboard(terse_handle_t handle, const char *data);
```

Writes text to system clipboard using OSC 52.

**Example:**
```c
const char *text = "Copied to clipboard!";
if (terse_set_clipboard(handle, text) == 0) {
    printf("Clipboard updated\n");
} else {
    printf("Clipboard not supported\n");
}
```

**Note:** Clipboard read is not supported (terminal limitation).

### Cursor Shape

```c
int terse_set_cursor_shape(terse_handle_t handle,
                           terse_cursor_shape_t shape,
                           int blinking);
```

**Cursor Shapes:**
```c
typedef enum terse_cursor_shape {
    TERSE_CURSOR_SHAPE_DEFAULT = 0,
    TERSE_CURSOR_SHAPE_BLOCK,
    TERSE_CURSOR_SHAPE_UNDERLINE,
    TERSE_CURSOR_SHAPE_BAR
} terse_cursor_shape_t;
```

**Example:**
```c
// Blinking bar cursor
terse_set_cursor_shape(handle, TERSE_CURSOR_SHAPE_BAR, 1);

// Steady block cursor
terse_set_cursor_shape(handle, TERSE_CURSOR_SHAPE_BLOCK, 0);

// Restore default
terse_set_cursor_shape(handle, TERSE_CURSOR_SHAPE_DEFAULT, 1);
```

### Image Display

```c
int terse_display_image(terse_handle_t handle,
                        const terse_image_request_t *request);
```

**Image Request Structure:**
```c
typedef struct terse_image_request {
    const unsigned char *data;  // Image data
    size_t size;                // Data size in bytes
    const char *name;           // Optional filename (can be NULL)
    terse_image_format_t format;// Image format
    int width;                  // Optional width hint (0 = auto)
    int height;                 // Optional height hint (0 = auto)
    unsigned int flags;         // Display flags
} terse_image_request_t;
```

**Image Formats:**
```c
typedef enum terse_image_format {
    TERSE_IMAGE_FORMAT_AUTO = 0,  // Auto-detect from data
    TERSE_IMAGE_FORMAT_PNG,
    TERSE_IMAGE_FORMAT_JPEG,
    TERSE_IMAGE_FORMAT_SIXEL,
    TERSE_IMAGE_FORMAT_KITTY
} terse_image_format_t;
```

**Image Flags:**
```c
enum {
    TERSE_IMAGE_FLAG_INLINE = 1 << 0,          // Display inline
    TERSE_IMAGE_FLAG_ALLOW_DEGRADE = 1 << 1    // Allow format degradation
};
```

**Supported Protocols:**
```c
typedef enum terse_image_support {
    TERSE_IMAGE_NONE = 0,
    TERSE_IMAGE_ITERM_INLINE,  // iTerm2 inline images
    TERSE_IMAGE_SIXEL,         // Sixel graphics
    TERSE_IMAGE_KITTY          // Kitty graphics protocol
} terse_image_support_t;
```

**Example:**
```c
// Load PNG image data
unsigned char *png_data;
size_t png_size;
// ... load image file ...

terse_image_request_t request = {
    .data = png_data,
    .size = png_size,
    .name = "example.png",
    .format = TERSE_IMAGE_FORMAT_PNG,
    .width = 0,   // Auto width
    .height = 0,  // Auto height
    .flags = TERSE_IMAGE_FLAG_INLINE | TERSE_IMAGE_FLAG_ALLOW_DEGRADE
};

if (terse_display_image(handle, &request) == 0) {
    printf("Image displayed\n");
} else {
    printf("Image display failed or not supported\n");
}
```

**Legacy API:**
```c
int terse_display_image_inline(terse_handle_t handle,
                               const unsigned char *data,
                               size_t size,
                               const char *name);
```

Simple wrapper around `terse_display_image()` for backward compatibility.

### Notifications

```c
int terse_notify(terse_handle_t handle,
                 terse_notification_kind_t kind,
                 const char *payload);
```

**Notification Types:**
```c
typedef enum terse_notification_kind {
    TERSE_NOTIFICATION_KIND_BELL = 0,     // Terminal bell
    TERSE_NOTIFICATION_KIND_VISUAL,       // Visual bell (flash)
    TERSE_NOTIFICATION_KIND_DESKTOP       // Desktop notification
} terse_notification_kind_t;
```

**Support Flags:**
```c
enum {
    TERSE_NOTIFICATION_SUPPORT_BELL = 1 << 0,
    TERSE_NOTIFICATION_SUPPORT_VISUAL = 1 << 1,
    TERSE_NOTIFICATION_SUPPORT_DESKTOP = 1 << 2
};
```

**Examples:**
```c
// Terminal bell
terse_notify(handle, TERSE_NOTIFICATION_KIND_BELL, NULL);

// Visual bell (screen flash)
terse_notify(handle, TERSE_NOTIFICATION_KIND_VISUAL, NULL);

// Desktop notification
terse_notify(handle, TERSE_NOTIFICATION_KIND_DESKTOP,
             "Build completed successfully!");

// Check support
terse_capabilities_t caps = terse_get_capabilities(handle);
if (caps.notifications & TERSE_NOTIFICATION_SUPPORT_DESKTOP) {
    terse_notify(handle, TERSE_NOTIFICATION_KIND_DESKTOP,
                 "Important message");
}
```

---

## Test Mode and Mocking

### Overview

Terse provides a test mode for automated testing and verification. When enabled at build time, the library records API calls and allows mocking of capabilities, terminal size, and input events.

### Building with Test Mode

```bash
cmake -S . -B build -DTERSE_ENABLE_TEST_MODE=ON -G Ninja
ninja -C build
```

**Note:** Test mode is disabled by default in production builds.

### Recording API Calls

Test mode can record all rendering and control API calls for verification:

```c
#ifdef TERSE_ENABLE_TEST_MODE
#include "terse_test.h"

// Start recording
terse_test_start_recording(handle);

// Execute rendering operations
terse_move_to(handle, 5, 10);
terse_write_text(handle, "Hello");
terse_clear_screen(handle, TERSE_CLEAR_ALL);

// Stop recording
terse_test_stop_recording(handle);

// Retrieve recorded calls
int count = 0;
const terse_call_record_t *calls = terse_test_get_calls(handle, &count);

// Verify calls
assert(count == 3);
assert(calls[0].type == TERSE_CALL_MOVE_TO);
assert(calls[0].data.move_to.row == 5);
assert(calls[0].data.move_to.col == 10);

assert(calls[1].type == TERSE_CALL_WRITE_TEXT);
assert(strcmp(calls[1].data.write_text.text, "Hello") == 0);

assert(calls[2].type == TERSE_CALL_CLEAR_SCREEN);
assert(calls[2].data.clear_screen.mode == TERSE_CLEAR_ALL);

// Clear recorded calls
terse_test_clear_calls(handle);
#endif
```

**Recorded Call Types:**
- `TERSE_CALL_WRITE_TEXT`
- `TERSE_CALL_MOVE_TO`
- `TERSE_CALL_CLEAR_SCREEN`
- `TERSE_CALL_CLEAR_LINE`
- `TERSE_CALL_SHOW_CURSOR`
- `TERSE_CALL_SET_STYLE`
- `TERSE_CALL_ENABLE_MOUSE`
- `TERSE_CALL_DISABLE_MOUSE`
- `TERSE_CALL_SET_TITLE`
- `TERSE_CALL_FLUSH`

### Mocking Capabilities

Override terminal capabilities for testing:

```c
#ifdef TERSE_ENABLE_TEST_MODE
// Create mock capabilities
terse_capabilities_t mock_caps = {
    .profile = TERSE_P1,
    .colors = TERSE_COLOR_BASIC16,
    .mouse = TERSE_MOUSE_NONE,
    .images = TERSE_IMAGE_NONE
};

// Apply mock
terse_test_mock_capabilities(handle, &mock_caps);

// Test behavior with limited capabilities
terse_capabilities_t caps = terse_get_capabilities(handle);
assert(caps.colors == TERSE_COLOR_BASIC16);
assert(caps.mouse == TERSE_MOUSE_NONE);

// Reset to actual capabilities
terse_test_reset_mocks(handle);
#endif
```

### Mocking Terminal Size

Test UI layout with different terminal sizes:

```c
#ifdef TERSE_ENABLE_TEST_MODE
// Mock 80x24 terminal
terse_test_mock_size(handle, 24, 80);

terse_size_t size = terse_get_size(handle);
assert(size.rows == 24);
assert(size.cols == 80);
assert(size.known == 1);

// Mock 120x40 terminal
terse_test_mock_size(handle, 40, 120);

size = terse_get_size(handle);
assert(size.rows == 40);
assert(size.cols == 120);

// Reset to actual size
terse_test_reset_mocks(handle);
#endif
```

### Mocking Input Events

Inject synthetic input events for testing event handling:

```c
#ifdef TERSE_ENABLE_TEST_MODE
// Prepare synthetic events
terse_event_t events[3];

// Event 1: Character 'a'
events[0].type = TERSE_EVENT_CHAR;
events[0].data.ch.scalar = 'a';
events[0].data.ch.width = 1;
events[0].data.ch.mods = 0;

// Event 2: Arrow Up with Shift
events[1].type = TERSE_EVENT_ARROW_UP;
events[1].data.key.mods = TERSE_MOD_SHIFT;

// Event 3: Resize to 25x80
events[2].type = TERSE_EVENT_RESIZE;
events[2].data.resize.rows = 25;
events[2].data.resize.cols = 80;

// Inject events
terse_test_mock_events(handle, events, 3);

// Read injected events
terse_event_t event;
assert(terse_read_event(handle, 0, &event) == TERSE_OK);
assert(event.type == TERSE_EVENT_CHAR);
assert(event.data.ch.scalar == 'a');

assert(terse_read_event(handle, 0, &event) == TERSE_OK);
assert(event.type == TERSE_EVENT_ARROW_UP);
assert(event.data.key.mods == TERSE_MOD_SHIFT);

assert(terse_read_event(handle, 0, &event) == TERSE_OK);
assert(event.type == TERSE_EVENT_RESIZE);
assert(event.data.resize.rows == 25);

// No more events
assert(terse_read_event(handle, 0, &event) == TERSE_ERR_NO_EVENT);

// Reset to real input
terse_test_reset_mocks(handle);
#endif
```

### Use Cases

**1. UI Regression Testing:**
```c
// Record baseline rendering
terse_test_start_recording(handle);
render_ui(handle);
terse_test_stop_recording(handle);
int baseline_count;
const terse_call_record_t *baseline = terse_test_get_calls(handle, &baseline_count);
// Save baseline for comparison...

// Later: verify no unexpected changes
terse_test_clear_calls(handle);
terse_test_start_recording(handle);
render_ui(handle);
terse_test_stop_recording(handle);
int current_count;
const terse_call_record_t *current = terse_test_get_calls(handle, &current_count);
assert(current_count == baseline_count);
// Compare call sequences...
```

**2. Capability Fallback Testing:**
```c
// Test with TrueColor
terse_capabilities_t truecolor_caps = {...};
truecolor_caps.colors = TERSE_COLOR_TRUECOLOR;
terse_test_mock_capabilities(handle, &truecolor_caps);
test_color_rendering(handle);

// Test with 16 colors
terse_capabilities_t basic_caps = {...};
basic_caps.colors = TERSE_COLOR_BASIC16;
terse_test_mock_capabilities(handle, &basic_caps);
test_color_rendering(handle);

// Test with no color
terse_capabilities_t no_color_caps = {...};
no_color_caps.colors = TERSE_COLOR_NONE;
terse_test_mock_capabilities(handle, &no_color_caps);
test_color_rendering(handle);
```

**3. Automated Event Handler Testing:**
```c
// Inject key sequence: "hello" + Enter
terse_event_t events[6];
events[0] = (terse_event_t){.type = TERSE_EVENT_CHAR, .data.ch.scalar = 'h'};
events[1] = (terse_event_t){.type = TERSE_EVENT_CHAR, .data.ch.scalar = 'e'};
events[2] = (terse_event_t){.type = TERSE_EVENT_CHAR, .data.ch.scalar = 'l'};
events[3] = (terse_event_t){.type = TERSE_EVENT_CHAR, .data.ch.scalar = 'l'};
events[4] = (terse_event_t){.type = TERSE_EVENT_CHAR, .data.ch.scalar = 'o'};
events[5] = (terse_event_t){.type = TERSE_EVENT_ENTER};

terse_test_mock_events(handle, events, 6);

// Run event loop and verify input buffer state
run_event_loop(handle);
assert(strcmp(input_buffer, "hello") == 0);
```

**Example:** See `samples/test_mode_demo.c` for a complete working example.

---

## Best Practices

### 1. Always Close the Handle

```c
terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
if (!handle) {
    return 1;
}

// ... use handle ...

terse_close(handle);  // Always call this
```

### 2. Check Capabilities Before Using Features

```c
terse_capabilities_t caps = terse_get_capabilities(handle);

if (caps.colors >= TERSE_COLOR_TRUECOLOR) {
    // Use TrueColor
} else {
    // Fallback to basic colors
}

if (caps.mouse >= TERSE_MOUSE_SGR) {
    terse_enable_mouse(handle, TERSE_MOUSE_SGR);
}
```

### 3. Use State Management for Temporary Changes

```c
// Save state
terse_push_state(handle);

// Temporary changes
terse_show_cursor(handle, 0);
terse_move_to(handle, 1, 1);
// ... draw popup ...

// Restore state
terse_pop_state(handle);
```

### 4. Handle Resize Events

```c
terse_size_t size = terse_get_size(handle);
int current_rows = size.rows;
int current_cols = size.cols;

while (1) {
    terse_event_t event;
    if (terse_read_event(handle, 100, &event) == TERSE_OK) {
        if (event.type == TERSE_EVENT_RESIZE) {
            current_rows = event.data.resize.rows;
            current_cols = event.data.resize.cols;
            // Redraw UI with new dimensions
        }
    }
}
```

### 5. Proper Error Handling

```c
if (terse_write_text(handle, "Hello") < 0) {
    terse_error_info_t err = terse_get_last_error(handle);
    fprintf(stderr, "Write failed: category=%d, code=%d\n",
            err.category, err.code);
    // Handle error appropriately
}
```

### 6. Use TERSE_PROFILE_AUTO for Portability

```c
// Automatically detect best profile for terminal
terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
```

### 7. Clean Up Resources in Correct Order

```c
// Disable features before closing
terse_disable_mouse(handle);
terse_disable_bracketed_paste(handle);
terse_show_cursor(handle, 1);
terse_reset_style(handle, TERSE_RESET_ALL);

// Then close
terse_close(handle);
```

### 8. Use UTF-8 by Default

```c
terse_options_t options = {
    .input_fd = STDIN_FILENO,
    .output_fd = STDOUT_FILENO,
    .codec_name = "UTF-8",  // Recommended
    .disabled_caps = 0,
    .enabled_caps = 0
};
```

### 9. Timeout Considerations

```c
// Non-blocking read
terse_read_event(handle, 0, &event);

// Short timeout for responsive UI
terse_read_event(handle, 100, &event);  // 100ms

// Blocking read
terse_read_event(handle, -1, &event);
```

### 10. Graceful Degradation Example

```c
void draw_colored_text(terse_handle_t handle, const char *text) {
    terse_capabilities_t caps = terse_get_capabilities(handle);

    if (caps.colors >= TERSE_COLOR_BASIC16) {
        terse_style_t style = terse_style_default();
        style.foreground = terse_color_basic(TERSE_BASIC_COLOR_GREEN, 1);
        terse_set_style(handle, &style);
        terse_write_text(handle, text);
        terse_reset_style(handle, TERSE_RESET_ALL);
    } else {
        // No color support
        terse_write_text(handle, text);
    }
}
```

### 11. Complete Application Template

```c
#include "terse.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    terse_options_t options = {
        .input_fd = STDIN_FILENO,
        .output_fd = STDOUT_FILENO,
        .codec_name = "UTF-8",
        .disabled_caps = 0,
        .enabled_caps = 0
    };

    terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
    if (!handle) {
        fprintf(stderr, "Failed to initialize terse\n");
        return 1;
    }

    // Setup
    terse_clear_screen(handle, TERSE_CLEAR_ALL);
    terse_show_cursor(handle, 0);

    terse_capabilities_t caps = terse_get_capabilities(handle);
    if (caps.mouse >= TERSE_MOUSE_SGR) {
        terse_enable_mouse(handle, TERSE_MOUSE_SGR);
    }

    // Main loop
    int running = 1;
    while (running) {
        terse_event_t event;
        int result = terse_read_event(handle, 100, &event);

        if (result == TERSE_OK) {
            if (event.type == TERSE_EVENT_CHAR &&
                event.data.ch.scalar == 'q') {
                running = 0;
            }
            // Handle other events...
        }

        // Update UI periodically
    }

    // Cleanup
    if (caps.mouse >= TERSE_MOUSE_SGR) {
        terse_disable_mouse(handle);
    }
    terse_show_cursor(handle, 1);
    terse_clear_screen(handle, TERSE_CLEAR_ALL);
    terse_close(handle);

    return 0;
}
```

---

## Advanced Topics

### Character Encoding

Terse supports two character encodings:

- **UTF-8** (recommended): Universal, modern encoding
- **Shift_JIS**: Japanese legacy encoding

**East Asian Width Handling:**

```c
terse_options_t options = {
    // ...
    .east_asian_ambiguous_as_wide = 1  // Treat ambiguous chars as 2 cells
};
```

Characters like `±`, `×`, `§` can be 1 or 2 cells wide depending on context. This option controls the behavior.

### Building Without System iconv

```bash
cmake -S . -B build -DTERSE_ENABLE_ICONV=OFF
```

Uses built-in mini iconv (Shift_JIS ↔ UTF-8 only).

### Terminal Detection

Terse automatically detects these terminals:

- **Apple Terminal**
- **GNOME Terminal / VTE**
- **iTerm2**
- **WezTerm**
- **kitty**
- **Ghostty**
- **Warp**

Detection uses environment variables (`TERM_PROGRAM`, `VTE_VERSION`, etc.) and Secondary Device Attributes (DA).

### Profile Clipping

When you request a specific profile, capabilities are clipped to that level:

```c
// Request P1, but terminal supports P3
terse_handle_t handle = terse_open(TERSE_P1, &options);

terse_capabilities_t caps = terse_get_capabilities(handle);
// caps.profile will be TERSE_P1
// P2/P3 features will be disabled
```

---

## Common Patterns

### Drawing a Box

```c
void draw_box(terse_handle_t handle, int row, int col,
              int width, int height) {
    const char *corner_tl = "┌";
    const char *corner_tr = "┐";
    const char *corner_bl = "└";
    const char *corner_br = "┘";
    const char *horizontal = "─";
    const char *vertical = "│";

    // Top border
    terse_move_to(handle, row, col);
    terse_write_text(handle, corner_tl);
    for (int i = 0; i < width - 2; i++) {
        terse_write_text(handle, horizontal);
    }
    terse_write_text(handle, corner_tr);

    // Sides
    for (int r = 1; r < height - 1; r++) {
        terse_move_to(handle, row + r, col);
        terse_write_text(handle, vertical);
        terse_move_to(handle, row + r, col + width - 1);
        terse_write_text(handle, vertical);
    }

    // Bottom border
    terse_move_to(handle, row + height - 1, col);
    terse_write_text(handle, corner_bl);
    for (int i = 0; i < width - 2; i++) {
        terse_write_text(handle, horizontal);
    }
    terse_write_text(handle, corner_br);
}
```

### Progress Bar

```c
void draw_progress_bar(terse_handle_t handle, int row, int col,
                       int width, float progress) {
    int filled = (int)(progress * width);

    terse_move_to(handle, row, col);
    terse_write_text(handle, "[");

    terse_style_t style = terse_style_default();
    style.foreground = terse_color_basic(TERSE_BASIC_COLOR_GREEN, 1);
    terse_set_style(handle, &style);

    for (int i = 0; i < filled; i++) {
        terse_write_text(handle, "█");
    }

    terse_reset_style(handle, TERSE_RESET_ALL);

    for (int i = filled; i < width; i++) {
        terse_write_text(handle, "░");
    }

    terse_write_text(handle, "]");
}
```

### Menu Selection

```c
void draw_menu(terse_handle_t handle, const char **items, int count,
               int selected) {
    for (int i = 0; i < count; i++) {
        terse_move_to(handle, 5 + i, 10);

        if (i == selected) {
            terse_style_t style = terse_style_default();
            style.effects = TERSE_STYLE_INVERSE;
            terse_set_style(handle, &style);
        }

        terse_write_text(handle, items[i]);

        if (i == selected) {
            terse_reset_style(handle, TERSE_RESET_ALL);
        }
    }
}
```

---

## Troubleshooting

### Terminal Not Detected Correctly

Check environment variables:
```bash
echo $TERM
echo $TERM_PROGRAM
echo $VTE_VERSION
```

Override detection with capability flags:
```c
options.enabled_caps = TERSE_CAP_ENABLE_TRUECOLOR;
```

### Mouse Events Not Working

1. Check if mouse is supported:
```c
terse_capabilities_t caps = terse_get_capabilities(handle);
if (caps.mouse == TERSE_MOUSE_NONE) {
    printf("Mouse not supported\n");
}
```

2. Ensure mouse is enabled:
```c
terse_enable_mouse(handle, TERSE_MOUSE_SGR);
```

3. Check if terminal is in raw mode (required for mouse input)

### Colors Not Displaying

Check color support:
```c
terse_capabilities_t caps = terse_get_capabilities(handle);
printf("Color support: %d\n", caps.colors);
// 0=none, 1=16, 2=256, 3=TrueColor
```

### Images Not Displaying

1. Check image support:
```c
terse_capabilities_t caps = terse_get_capabilities(handle);
if (caps.images == TERSE_IMAGE_NONE) {
    printf("Images not supported\n");
}
```

2. Verify image data is valid PNG/JPEG

3. Check terminal (iTerm2, WezTerm, kitty support images)

---

## API Reference Summary

### Lifecycle
- `terse_open()` - Open session
- `terse_close()` - Close session
- `terse_validate_options()` - Validate options

### Capabilities
- `terse_get_capabilities()` - Query capabilities
- `terse_capabilities_enable()` - Enable features
- `terse_capabilities_disable()` - Disable features
- `terse_capabilities_reset_overrides()` - Reset overrides

### Output (P0)
- `terse_clear_screen()` - Clear screen
- `terse_clear_line()` - Clear line
- `terse_move_to()` - Absolute cursor movement
- `terse_move_by()` - Relative cursor movement
- `terse_show_cursor()` - Show/hide cursor
- `terse_write_text()` - Write text
- `terse_flush()` - Flush output (no-op)

### Input (P0)
- `terse_read_event()` - Read input event
- `terse_keyboard_enable()` - Enable keyboard features
- `terse_keyboard_disable()` - Disable keyboard features

### Query (P0)
- `terse_get_size()` - Get terminal size
- `terse_get_cursor_position()` - Get cursor position
- `terse_get_options()` - Get current options
- `terse_get_last_error()` - Get last error

### State (P0)
- `terse_capture_state()` - Capture state
- `terse_restore_state()` - Restore state
- `terse_push_state()` - Push state to stack
- `terse_pop_state()` - Pop state from stack
- `terse_state_override()` - Override state
- `terse_state_clear()` - Clear override

### Colors/Styles (P1)
- `terse_color_default()` - Default color
- `terse_color_basic()` - Basic 16 colors
- `terse_color_palette()` - 256-color palette
- `terse_color_truecolor()` - 24-bit RGB
- `terse_style_default()` - Default style
- `terse_set_style()` - Apply style
- `terse_reset_style()` - Reset style

### Mouse/Input (P2)
- `terse_enable_mouse()` - Enable mouse
- `terse_disable_mouse()` - Disable mouse
- `terse_enable_bracketed_paste()` - Enable paste mode
- `terse_disable_bracketed_paste()` - Disable paste mode

### Window (P2)
- `terse_set_title()` - Set window title
- `terse_set_hyperlink()` - Create hyperlink

### Extended (P3)
- `terse_set_clipboard()` - Write to clipboard
- `terse_set_cursor_shape()` - Set cursor shape
- `terse_display_image()` - Display image
- `terse_display_image_inline()` - Display image (legacy)
- `terse_notify()` - Send notification

---

## Additional Resources

- Specification: `docs/terse-specs.md`
- Progress overview: `docs/progress-overview.md`
- Graphics roadmap: `docs/graphics-roadmap.md`
- Platform porting: `docs/terse-platform-porting.md`
- Mini iconv implementation: `docs/mini-iconv-plan.md`

---

**End of Terse API User Guide**
